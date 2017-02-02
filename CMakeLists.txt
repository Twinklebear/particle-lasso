cmake_minimum_required(VERSION 2.8.12)
project(PARTICLE_TO_IDX)

# Bump up warning levels appropriately for each compiler
if (UNIX OR APPLE OR MINGW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++14")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -m64 -ggdb -DDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -m64 -O3 -march=native -DNDEBUG")
elseif (MSVC)
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /Zi")
	endif()
endif()

# The visus package script isn't properly written so
# we need to explicitly include the visus lib dir to link
# against the Visus libraries easily.
find_package(Visus REQUIRED)
link_directories(${VISUS_LINK_DIRECTORIES})

# Small hack: install the visus shared libs on Windows to our exe dir
if (MSVC)
	set(VISUS_SHARED_LIBRARIES "${VISUS_LINK_DIRECTORIES}/VisusIdx.dll"
		"${VISUS_LINK_DIRECTORIES}/VisusKernel.dll"
		"${VISUS_LINK_DIRECTORIES}/VisusDb.dll")
	string(REGEX REPLACE "lib" "bin" VISUS_SHARED_LIBRARIES "${VISUS_SHARED_LIBRARIES}")
else ()
	set(VISUS_SHARED_LIBRARIES "${VISUS_LINK_DIRECTORIES}/libVisusIdx.so"
		"${VISUS_LINK_DIRECTORIES}/libVisusKernel.so"
		"${VISUS_LINK_DIRECTORIES}/libVisusDb.so")
endif()

# Small hack to copy the visus shared libs to my executable's dir
install(FILES ${VISUS_SHARED_LIBRARIES} DESTINATION bin)

include_directories(include ${VISUS_INCLUDE_DIRECTORIES})
add_subdirectory(src)

option(PARTICLE_IDX_VL_PLUGIN OFF "Build the vislight plugin")
if (PARTICLE_IDX_VL_PLUGIN)
	get_plugin_name(plugin_name "particle_idx")
	add_library(${plugin_name} MODULE particle_idx_plugin.cpp)
	target_link_libraries(${plugin_name} vl VisusIdx VisusKernel VisusDb)
	install(TARGETS ${plugin_name} DESTINATION bin)
endif()


