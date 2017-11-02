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

add_subdirectory(src)

option(PARTICLE_IDX_VL_PLUGIN OFF "Build the vislight plugin")
if (PARTICLE_IDX_VL_PLUGIN)
	get_plugin_name(plugin_name "particle_idx")
	get_plugin_resource_dir(plugin_res_dir "particle_idx")
	add_library(${plugin_name} MODULE particle_idx_plugin.cpp)
	target_link_libraries(${plugin_name} vl)
	install(TARGETS ${plugin_name} DESTINATION bin)
	install(FILES particle_idx_frag.glsl particle_idx_vert.glsl
		DESTINATION ${plugin_res_dir})
endif()


