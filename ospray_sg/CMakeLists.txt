find_package(ospray REQUIRED)

add_library(ospray_module_particle_lasso SHARED
	particle_lasso_module.cpp)
target_include_directories(ospray_module_particle_lasso PRIVATE
	${OSPRAY_INCLUDE_DIRS})
target_link_libraries(ospray_module_particle_lasso PRIVATE
	particle_lasso
	${OSPRAY_LIBRARIES}
	${OSPRAY_SG_LIBRARIES})
target_compile_definitions(particle_lasso PRIVATE
	-DNOMINMAX -DWIN32_LEAN_AND_MEAN)

set_target_properties(ospray_module_particle_lasso
	PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON)

