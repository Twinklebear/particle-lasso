cmake_minimum_required(VERSION 2.6)
project(POINT_TO_IDX)

# Bump up warning levels appropriately for each compiler
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic --std=c++14")
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
find_package(LAStools REQUIRED)

if(NOT LASTOOLS_FOUND)
	message(FATAL_ERROR "Could not find LAStools (did you set $LASTOOLS_DIR?)")
endif()

include_directories(include ${LASTOOLS_INCLUDE_DIRS})
add_subdirectory(src)

