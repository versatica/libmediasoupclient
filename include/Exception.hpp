#ifndef MSC_EXCEPTION_HPP
#define MSC_EXCEPTION_HPP

#include <exception>
#include <string>
#include <utility>

namespace mediasoupclient
{
class Exception : public std::exception
{
public:
	explicit Exception(std::string error) : error(std::move(error))
	{
	}

	const char* what() const noexcept override
	{
		return this->error.c_str();
	}

private:
	std::string error;
};
} // namespace mediasoupclient

#endif
