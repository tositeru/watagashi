cmake_minimum_required(VERSION 3.10)
#set(Boost_DEBUG ON)

# guard against in-source builds
#if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
#  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
#endif()

project(Watagashi    LANGUAGES CXX
    VERSION 0.1.0
)

find_package(Threads REQUIRED)

# Boost configuration

if (MSVC) # use 32bit or 64bit libraries properly correctly
  set(CMAKE_LIBRARY_ARCHITECTURE "${CMAKE_VS_PLATFORM_NAME}")
endif (MSVC)

set(Boost_USE_STATIC_LIBS        ON)
find_package(Boost 1.66.0 REQUIRED
  COMPONENTS system filesystem program_options)
message("## Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message("## Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")

add_subdirectory(src)

