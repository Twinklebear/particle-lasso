option(PARTICLE_LASSO_ENABLE_LIDAR "Build Lidar data support, via LAStools" OFF)

set(LASSO_SRC import_scivis16.cpp import_xyz.cpp
	import_uintah.cpp tinyxml2.cpp types.cpp)

set(LASSO_HEADERS import_scivis16.h import_xyz.h
	import_uintah.h tinyxml2.h types.h)

configure_file(particle_lasso_cfg.h.in particle_lasso_cfg.h)

if (PARTICLE_LASSO_ENABLE_LIDAR)
	find_package(LAStools REQUIRED)
	set(LASSO_SRC ${LASSO_SRC} import_las.cpp)
	set(LASSO_HEADERS ${LASSO_HEADERS} import_las.h)
endif()

add_library(particle_lasso ${LASSO_SRC})
target_include_directories(particle_lasso PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<INSTALL_INTERFACE:include/particle_lasso>)

if (PARTICLE_LASSO_ENABLE_LIDAR)
	target_include_directories(particle_lasso PUBLIC ${LASTOOLS_INCLUDE_DIRS})
	target_link_libraries(particle_lasso PUBLIC ${LASTOOLS_LIBRARIES})
endif()

set_property(TARGET particle_lasso PROPERTY POSITION_INDEPENDENT_CODE ON)

install(TARGETS particle_lasso EXPORT particle_lassoConfig
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
	INCLUDES DESTINATION include
)
install(FILES
	${LASSO_HEADERS}
	${CMAKE_CURRENT_BINARY_DIR}/particle_lasso_cfg.h
	DESTINATION include/particle_lasso
)
install(EXPORT particle_lassoConfig
	DESTINATION lib/cmake/particle_lasso
)

