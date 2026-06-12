#include "ProtooClient.hpp"

#include <iostream>
#include <stdexcept>
#include <thread>

ProtooClient::ProtooClient(
  const std::string& url,
  const std::string& origin,
  NotificationCallback notificationCb,
  RequestCallback requestCb)
  : notificationCb(std::move(notificationCb)), requestCb(std::move(requestCb))
{
	this->ws.setUrl(url);

	// Protoo requires this subprotocol header.
	this->ws.addSubProtocol("protoo");

	// The mediasoup-demo server validates the Origin header against its domain.
	// clang-format off
	this->ws.setExtraHeaders(
	  {
	    { "Origin", origin },
	  });
	// clang-format on

	// Disable TLS cert verification for local self-signed certs.
	ix::SocketTLSOptions tlsOpts;
	tlsOpts.disable_hostname_validation = true;
	tlsOpts.caFile                      = "NONE"; // skip CA verification
	this->ws.setTLSOptions(tlsOpts);

	this->ws.setOnMessageCallback(
	  [this](const ix::WebSocketMessagePtr& msg)
	  {
		  if (msg->type == ix::WebSocketMessageType::Message)
		  {
			  this->OnMessage(msg->str);
		  }
		  else if (msg->type == ix::WebSocketMessageType::Open)
		  {
			  std::lock_guard<std::mutex> lock(this->connMutex);
			  this->connected = true;
			  this->connCv.notify_all();
		  }
		  else if (msg->type == ix::WebSocketMessageType::Error)
		  {
			  std::lock_guard<std::mutex> lock(this->connMutex);
			  this->connectError = msg->errorInfo.reason;
			  this->connected    = true; // unblock waitForConnection
			  this->connCv.notify_all();
		  }
	  });
}

void ProtooClient::Connect(int timeoutMs)
{
	this->ws.start();

	std::unique_lock<std::mutex> lock(this->connMutex);
	this->connCv.wait_for(
	  lock,
	  std::chrono::milliseconds(timeoutMs),
	  [this]
	  {
		  return this->connected;
	  });

	if (!this->connectError.empty())
	{
		throw std::runtime_error("WebSocket connection failed: " + this->connectError);
	}

	if (!this->connected)
	{
		throw std::runtime_error("WebSocket connection timed out");
	}
}

void ProtooClient::Close()
{
	this->ws.stop();
}

json ProtooClient::Request(const std::string& method, const json& data)
{
	const int id = this->nextId++;

	// clang-format off
	json msg = {
		{ "request", true   },
		{ "id",      id     },
		{ "method",  method },
		{ "data",    data   },
	};
	// clang-format on

	std::promise<json> promise;
	auto future = promise.get_future();

	{
		std::lock_guard<std::mutex> lock(this->pendingMutex);
		this->pending[id] = std::move(promise);
	}

	this->ws.sendText(msg.dump());

	auto status = future.wait_for(std::chrono::seconds(10));

	if (status != std::future_status::ready)
	{
		throw std::runtime_error("Protoo request timed out: " + method);
	}

	return future.get(); // throws if response was an error
}

void ProtooClient::OnMessage(const std::string& str)
{
	json msg;

	try
	{
		msg = json::parse(str);
	}
	catch (const std::exception& e)
	{
		std::cerr << "[protoo] failed to parse message: " << e.what() << "\n";

		return;
	}

	if (msg.value("response", false))
	{
		const int id = msg["id"];
		std::lock_guard<std::mutex> lock(this->pendingMutex);
		auto it = this->pending.find(id);

		if (it == this->pending.end())
		{
			return;
		}

		if (msg.value("ok", false))
		{
			it->second.set_value(msg.value("data", json::object()));
		}
		else
		{
			it->second.set_exception(
			  std::make_exception_ptr(std::runtime_error(msg.value("errorReason", "unknown error"))));
		}

		this->pending.erase(it);
	}
	else if (msg.value("notification", false))
	{
		this->notificationCb(msg["method"], msg.value("data", json::object()));
	}
	else if (msg.value("request", false))
	{
		// Server-initiated request.
		// IMPORTANT: dispatch to a worker thread so we don't block the ixwebsocket
		// receive thread. Blocking it (e.g. via a nested protoo.request() inside
		// ConsumeData → OnConnect) would deadlock because the thread can't receive
		// the response for the nested request while it's blocked.
		const int id             = msg["id"];
		const std::string method = msg["method"];
		const json data          = msg.value("data", json::object());

		std::thread(
		  [this, id, method, data]()
		  {
			  json responseData = json::object();
			  try
			  {
				  this->requestCb(method, data, responseData);

				  // clang-format off
				  json response = {
					  { "response", true         },
					  { "id",       id           },
					  { "ok",       true         },
					  { "data",     responseData },
				  };
				  // clang-format on
				  this->ws.sendText(response.dump());
			  }
			  catch (const std::exception& e)
			  {
				  std::cerr << "[protoo] request '" << method << "' handler error: " << e.what() << "\n";
				  // clang-format off
				  json response = {
					  { "response",    true     },
					  { "id",          id       },
					  { "ok",          false    },
					  { "errorReason", e.what() },
				  };
				  // clang-format on
				  this->ws.sendText(response.dump());
			  }
		  })
		  .detach();
	}
}
