option(PARTICLE_LASSO_ENABLE_LIDAR "Build Lidar data support, via LAStools" OFF)

set(LASSO_SRC import_scivis16.cpp import_xyz.cpp
	import_uintah.cpp
    tinyxml2.cpp
    types.cpp
	import_cosmic_web.cpp
    import_pkd.cpp
	import_gromacs.cpp
	particle_lasso.cpp
    import_libbat_bpf.cpp)

set(LASSO_HEADERS import_scivis16.h import_xyz.h
	import_uintah.h tinyxml2.h types.h particle_lasso.h
	import_cosmic_web.h import_pkd.h import_gromacs.h
    import_libbat_bpf.h json.hpp)

configure_file(particle_lasso_cfg.h.in particle_lasso_cfg.h)

if (PARTICLE_LASSO_ENABLE_LIDAR)
	find_package(LAStools REQUIRED)
	set(LASSO_SRC ${LASSO_SRC} import_las.cpp)
	set(LASSO_HEADERS ${LASSO_HEADERS} import_las.h)
endif()

add_library(particle_lasso STATIC ${LASSO_SRC})
target_include_directories(particle_lasso PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/particle_lasso>)

if (PARTICLE_LASSO_ENABLE_LIDAR)
	target_include_directories(particle_lasso PUBLIC ${LASTOOLS_INCLUDE_DIRS})
	target_link_libraries(particle_lasso PUBLIC ${LASTOOLS_LIBRARIES})
    target_compile_definitions(particle_lasso PUBLIC
        -DPARTICLE_LASSO_ENABLE_LIDAR=1)
endif()

set_target_properties(particle_lasso
	PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON)

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

add_executable(point_to_raw point_to_raw.cpp)
target_link_libraries(point_to_raw particle_lasso)
set_target_properties(point_to_raw
	PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON)

add_executable(point_to_duong_vtu point_to_duong_vtu.cpp)
target_link_libraries(point_to_duong_vtu particle_lasso)
set_target_properties(point_to_duong_vtu
	PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON)

