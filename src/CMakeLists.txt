cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D\"_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING\"")
  add_compile_options("/source-charset:utf-8") # Visual Studio 2015, or greater
  add_compile_options("/D \"NOMINMAX\"")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic")
endif()

add_executable(watagashi
)
target_sources(watagashi
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/builder.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/builder.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/errorReceiver.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/errorReceiver.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/exception.hpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/includeFileAnalyzer.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/includeFileAnalyzer.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/processServer.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/processServer.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/programOptions.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/programOptions.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utility.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utility.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/data.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/data.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/source.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/source.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/indent.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/indent.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/line.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/line.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/enviroment.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/enviroment.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/parserUtility.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/parserUtility.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/parseMode.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/parseMode.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/normal.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/normal.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/multiLineComment.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/multiLineComment.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/objectDefined.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/objectDefined.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/boolean.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/boolean.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/branch.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/branch.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/doNothing.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/defineFunction.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/mode/defineFunction.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/scope.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/scope.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/value.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/parser/value.cpp"
)

include_directories(
  "${Boost_INCLUDE_DIRS}")

target_link_libraries(watagashi
  Boost::system
  Boost::filesystem
  Boost::program_options
  Threads::Threads)

# install settings
include(GNUInstallDirs)

install(TARGETS watagashi
  EXPORT watagashi
  RUNTIME DESTINATION CMAKE_INSTALL_BINDIR)

