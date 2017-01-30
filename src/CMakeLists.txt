add_executable(particle_idx particle_idx.cpp)
message("visus kernel libs ${VISUS_KERNEL_LINK_LIBRARIES}")
target_link_libraries(particle_idx ${VISUS_KERNEL_LINK_LIBRARIES} ${VISUS_LIBRARIES})
install(TARGETS particle_idx DESTINATION .)

