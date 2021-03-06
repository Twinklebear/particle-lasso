cmake_minimum_required(VERSION 3.5)
project(PARTICLE_LASSO)

# Bump up warning levels appropriately for each compiler
if (UNIX OR APPLE OR MINGW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -m64 -ggdb -DDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -m64 -O3 -DNDEBUG")
elseif (MSVC)
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /Zi")
	endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(PARTICLE_LASSO_ENABLE_LIDAR "Build Lidar data support, via LAStools" OFF)
if (PARTICLE_LASSO_ENABLE_LIDAR)
	find_package(LAStools REQUIRED)
endif()

add_subdirectory(src)

option(PARTICLE_LASSO_OSPRAY_IMPORTER
	"Build the OSPRay Example Viewer scenegraph importer" OFF)
if (PARTICLE_LASSO_OSPRAY_IMPORTER)
	add_subdirectory(ospray_sg)
endif()

