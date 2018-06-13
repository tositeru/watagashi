#pragma once

#include <vector>
#include <string>

namespace watagashi {

class ErrorReceiver
{
public:
	enum MessageType
	{
		eError = 1,
		eWarnning,
	};

public:
	ErrorReceiver();
	~ErrorReceiver();

	void clear();

	void receive(MessageType type, size_t line, const std::string& message);

	bool empty()const;

	bool hasError()const;

	void print(uint32_t flag = static_cast<uint32_t>(-1));

private:
	struct Message
	{
		MessageType type;
		std::string message;
	};

private:
	std::string attachMessageTypePrefix(const std::string& str, MessageType type, size_t line)const;

private:
	std::vector<Message> mMessages;
};

}
