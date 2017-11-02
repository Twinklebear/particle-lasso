option(PARTICLE_IDX_VL_PLUGIN OFF "Build the vislight plugin")
if (PARTICLE_IDX_VL_PLUGIN)
	add_executable(particle_idx particle_idx.cpp)
	target_link_libraries(particle_idx VisusIdx VisusKernel VisusDb)
	install(TARGETS particle_idx DESTINATION bin)

	get_plugin_name(plugin_name "particle_idx")
	get_plugin_resource_dir(plugin_res_dir "particle_idx")
	add_library(${plugin_name} MODULE particle_idx_plugin.cpp)
	target_link_libraries(${plugin_name} vl VisusIdx VisusKernel VisusDb)
	install(TARGETS ${plugin_name} DESTINATION bin)
	install(FILES particle_idx_frag.glsl particle_idx_vert.glsl
		DESTINATION ${plugin_res_dir})
endif()

