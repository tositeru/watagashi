#pragma once

#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <boost/stacktrace.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/exception/enable_error_info.hpp>
#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>
#include <boost/current_function.hpp>


#ifdef WIN32
#include <Windows.h>
#include <Dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")

#endif

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
            << StackTraceErrorInfo(std::move(this->stacktrace))    ;
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

#define AWESOME_THROW(...)             \
::ExceptionThrower<__VA_ARGS__>(    \
    BOOST_CURRENT_FUNCTION,            \
    __FILE__,                                \
    __LINE__,                                \
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
        std::exception_ptr const pException = std::current_exception();
        if(!pException) {
            std::abort();
        }
        try {
            std::rethrow_exception(pException);
        } catch(boost::exception const& e) {
            {
                std::cerr << "terminate called after throwing an instance of '";
                std::type_info const& ti = typeid(e);
#ifdef WIN32
                char undecoratedName[1024];
                auto ret = UnDecorateSymbolName(ti.name(), undecoratedName, sizeof(undecoratedName), UNDNAME_COMPLETE);
                std::cerr << (ret != 0 ? undecoratedName : ti.name()) << "'\n";
#else
                int status;
                std::unique_ptr<char, void(*)(void*)> p(abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status), std::free);
                std::cerr << (status == 0 ? p.get() : ti.name()) << "'\n";
#endif
            }
            if(char const *const* p = boost::get_error_info<boost::throw_file>(e) ) {
                std::cerr << *p << ':';
            }
            if(int const * p = boost::get_error_info<boost::throw_line>(e)) {
                std::cerr << *p << ":";
            }
            if(char const* const* p = boost::get_error_info<boost::throw_function>(e)) {
                std::cerr << *p << ": ";
            }
            if(std::exception const* p = dynamic_cast<std::exception const *>(&e)) {
                std::cerr << p->what();
            }
            std::cerr << "\n";
            if(boost::stacktrace::stacktrace const* p = boost::get_error_info<StackTraceErrorInfo>(e)) {
                std::cerr << "Backtrace:\n";
                std::cerr << *p << "\n";
            }
            std::cerr << std::flush;
        }catch(std::exception const& e) {
            std::cerr << "A unhandling exception occurred.\n";
            std::cerr << e.what() << std::endl;
        }catch(...) {
            std::cerr << "A unhandling exception occurred." << std::endl;
        }
        std::abort();
    }
};
