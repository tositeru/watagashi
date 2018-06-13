#pragma once

#include <iostream>
#include <ostream>
#include <sstream>
#include <boost/stacktrace/stacktrace.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/exception/enable_error_info.hpp>
#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>
#include <boost/current_function.hpp>

typedef boost::error_info<struct StackTraceErrorInfoTag, boost::stacktrace::stacktrace> StackTraceErrorInfo;

template<typename T>
class ExceptionThrower
{
public:
	ExceptionThrower(
		const char* functionName,
		const char* fileName,
		int lineNumber,
		boost::stacktrace::stacktrace && st)
		: oss()
		, functionName(functionName)
		, fileName(fileName)
		, lineNumber(lineNumber)
		, stacktrace(st)
	{}
	ExceptionThrower(const ExceptionThrower&) = delete;
	ExceptionThrower& operator=(const ExceptionThrower&) = delete;
	~ExceptionThrower()noexcept(false)
	{
		throw boost::enable_error_info(T(this->oss.str()))
			<< boost::throw_function(this->functionName)
			<< boost::throw_file(this->fileName)
			<< boost::throw_line(this->lineNumber)
			<< StackTraceErrorInfo(std::move(this->stacktrace))	;
	}
	
	template<typename U>
	std::ostream& operator<<(U &&x)
	{
		return this->oss << std::forward<U>(x);
	}
	
	template<typename U>
	std::ostream& operator<<(const U& x)
	{
		return this->oss << x;
	}
	
private:
	std::ostringstream oss;
	const char* functionName;
	const char* fileName;
	int lineNumber;
	boost::stacktrace::stacktrace stacktrace;
};

#define AWESOME_THROW(...) 			\
::ExceptionThrower<__VA_ARGS__>(	\
	BOOST_CURRENT_FUNCTION,			\
	__FILE__,								\
	__LINE__,								\
	::boost::stacktrace::stacktrace())

class ExceptionHandlerSetter
{
public:
	ExceptionHandlerSetter()noexcept
	{
		std::set_terminate(&terminate);
	}
private:
	[[noreturn]] static void terminate() noexcept
	{
		const std::exception_ptr p = std::current_exception();
		if(!p) {
			std::abort();
		}
		try {
			std::rethrow_exception(p);
		} catch(const boost::exception& e) {
			{
				int status;
				const std::type_info& ti = typeid(e);
				std::unique_ptr<char, void(*)(void*)> p(abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status), std::free);
				std::cerr << "terminate called after throwing an instance of '"
					<< (status == 0 ? p.get() : ti.name()) << "'\n";
			}
			if(const char*const* p = boost::get_error_info<boost::throw_file>(e) ) {
				std::cerr << *p << ':';
			}
			if(const int* p = boost::get_error_info<boost::throw_line>(e)) {
				std::cerr << *p << ":";
			}
			if(const char* const* p = boost::get_error_info<boost::throw_function>(e)) {
				std::cerr << *p << ": ";
			}
			if(const std::exception* p = dynamic_cast<const std::exception*>(&e)) {
				std::cerr << p->what();
			}
			std::cerr << "\n";
			if(const boost::stacktrace::stacktrace* p = boost::get_error_info<StackTraceErrorInfo>(e)) {
				std::cerr << "Backtrace:\n";
				std::cerr << *p << "\n";
			}
			std::cerr << std::flush;
		}catch(const std::exception& e) {
			std::cerr << "A unhandling exception occurred.\n";
			std::cerr << e.what() << std::endl;
		}catch(...) {
			std::cerr << "A unhandling exception occurred." << std::endl;
		}
		std::abort();
	}
};
