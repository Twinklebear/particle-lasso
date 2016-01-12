add_executable(point_to_raw point_to_raw.cpp types.cpp import_las.cpp
	tinyxml2.cpp)
target_link_libraries(point_to_raw ${LIBS})
install(TARGETS point_to_raw DESTINATION ${POINT_TO_IDX_INSTALL_DIR})

