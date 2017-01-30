add_executable(particle_idx particle_idx.cpp)
target_link_libraries(particle_idx VisusIdx VisusKernel VisusDb
	${VISUS_KERNEL_LINK_LIBRARIES})
install(TARGETS particle_idx DESTINATION .)

