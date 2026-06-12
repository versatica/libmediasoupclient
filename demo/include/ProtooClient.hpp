#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <ixwebsocket/IXWebSocket.h>
#include <json.hpp>
#include <map>
#include <mutex>
#include <string>

using json = nlohmann::json;

/**
 * Minimal Protoo WebSocket client.
 *
 * Protocol:
 *   Request:      { "request": true,  "id": N, "method": "...", "data": {} }
 *   Response OK:  { "response": true, "id": N, "ok": true, "data": {} }
 *   Response ERR: { "response": true, "id": N, "ok": false, "errorReason": "..." }
 *   Notification: { "notification": true, "method": "...", "data": {} }
 */
class ProtooClient
{
public:
	using NotificationCallback = std::function<void(const std::string& method, const json& data)>;
	using RequestCallback =
	  std::function<void(const std::string& method, const json& data, json& responseData)>;

	explicit ProtooClient(
	  const std::string& url,
	  const std::string& origin,
	  NotificationCallback notificationCb,
	  RequestCallback requestCb);

	void Connect(int timeoutMs = 5000);
	void Close();

	// Send a Protoo request and block until the response arrives.
	json Request(const std::string& method, const json& data = json::object());

private:
	void OnMessage(const std::string& str);

	ix::WebSocket ws;

	// Atomic: incremented from Request() on any calling thread.
	std::atomic<int> nextId{ 1 };

	// Guards connected/connectError: written by the ixwebsocket callback thread,
	// read by Connect() on the calling thread.
	std::mutex connMutex;
	std::condition_variable connCv;
	bool connected{ false };
	std::string connectError;

	// Guards pending: Request() (any thread) inserts, OnMessage() (ixwebsocket
	// thread) finds and erases — concurrent access to std::map requires a lock.
	std::mutex pendingMutex;
	std::map<int, std::promise<json>> pending;

	NotificationCallback notificationCb;
	RequestCallback requestCb;
};
