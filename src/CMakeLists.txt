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
  add_compile_options("/source-charset:utf-8") # Visual Studio 2015, or greater
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic")
endif()

add_executable(watagashi
)
target_sources(watagashi
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/buildEnviroment.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/buildEnviroment.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/builder.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/builder.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/config.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/config.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/configJsonParser.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/configJsonParser.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/errorReceiver.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/errorReceiver.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/exception.hpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/includeFileAnalyzer.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/includeFileAnalyzer.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/json11/json11.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/json11/json11.hpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/processServer.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/processServer.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/programOptions.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/programOptions.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/specialVariables.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/specialVariables.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utility.cpp"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utility.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/data.h"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/data.cpp"
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

