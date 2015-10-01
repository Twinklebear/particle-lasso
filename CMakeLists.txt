cmake_minimum_required(VERSION 2.6)
project(POINT_TO_IDX)

# Bump up warning levels appropriately for each compiler
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic --std=c++11")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -m64 -ggdb -DDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -m64 -O3 -march=native -DNDEBUG")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /Zi")
	endif()
endif()

set(POINT_TO_IDX_INSTALL_DIR "${POINT_TO_IDX_SOURCE_DIR}/bin")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${POINT_TO_IDX_SOURCE_DIR}/cmake")
find_package(LAS REQUIRED)
# It seems this lets it find the modern Boost libraries, since now it defaults to
# multithreaded libs but doesn't include the -mt suffix. Maybe since this is an old
# CMake version (2.6) it's not updated to recognize this change?
set(Boost_USE_MULTITHREADED OFF)
find_package(Boost 1.53 COMPONENTS program_options thread system iostreams filesystem REQUIRED)
if(NOT LAS_FOUND)
	message(FATAL_ERROR "Could not find libLAS (did you set $LAS_DIR if it's somewhere non-standard?)")
endif()
link_directories(${Boost_LIBRARY_DIRS})

set(LIBS ${LAS_LIBRARY} ${Boost_LIBRARIES})

include_directories(include ${LAS_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})
add_subdirectory(src)

