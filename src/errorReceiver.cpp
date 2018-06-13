#include "errorReceiver.h"

#include <iostream>
 
using namespace std;

namespace watagashi
{

ErrorReceiver::ErrorReceiver(){}
ErrorReceiver::~ErrorReceiver(){}

void ErrorReceiver::clear()
{
	this->mMessages.clear();
}

void ErrorReceiver::receive(
	MessageType type,
	size_t line,
	const std::string& message)
{
	Message msg;
	msg.type = type;
	msg.message = this->attachMessageTypePrefix(message, type, line);
	//msg.message = this->attachTag(msg.message);
	this->mMessages.push_back(msg);
}

bool ErrorReceiver::empty()const
{
	return this->mMessages.empty();
}

bool ErrorReceiver::hasError()const
{
	for(auto& msg : this->mMessages) {
		if(eError == msg.type)
			return true;
	}
	return false;
}

void ErrorReceiver::print(uint32_t flag)
{
	for(auto& msg : this->mMessages) {
		if(msg.type & flag) {
			cerr << msg.message << endl;
		}
	}
}

std::string ErrorReceiver::attachMessageTypePrefix(
	const std::string& str,
	MessageType type,
	size_t line)const
{
	std::string lineStr = "(line:" + std::to_string(line) + ") ";
	switch(type) {
	case eError: return lineStr + "!!error!! " + str;
	case eWarnning: return lineStr + "!!warnning!! " + str;
	default: return lineStr + "!!implement miss!! " + str;
	}
}

}
