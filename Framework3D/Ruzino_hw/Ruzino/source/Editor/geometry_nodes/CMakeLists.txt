# find OpenMP support
find_package(OpenMP REQUIRED)
message(STATUS "OpenMP found: ${OpenMP_FOUND}")
message(STATUS "OpenMP CXX flags: ${OpenMP_CXX_FLAGS}")
message(STATUS "OpenMP CXX include dirs: ${OpenMP_CXX_INCLUDE_DIR}")
message(STATUS "OpenMP libraries: ${OpenMP_CXX_LIBRARIES}")

# For MSVC, manually set OpenMP flag if find_package doesn't work correctly
if(MSVC)
    if("${OpenMP_CXX_FLAGS}" STREQUAL "-openmp" OR "${OpenMP_CXX_FLAGS}" STREQUAL "")
        message(STATUS "MSVC detected, manually setting OpenMP flag to /openmp")
        set(OpenMP_CXX_FLAGS "/openmp")
    endif()
endif()

# Build compile definitions list
set(GEOMETRY_NODES_COMPILE_DEFS NOMINMAX)
if(ENABLE_GEOM_USD_EXTENSION)
	list(APPEND GEOMETRY_NODES_COMPILE_DEFS GEOM_USD_EXTENSION=1)
endif()

# Add shader directory definition
list(APPEND GEOMETRY_NODES_COMPILE_DEFS GEOM_NODES_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/")

# Add mass_spring subdirectory to build MassSpring and FastMassSpring implementations
add_subdirectory(mass_spring)

add_nodes(
	TARGET_NAME geometry_nodes 
	SRC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
	DEP_LIBS stage nodes_system usd geometry usdShade Eigen3::Eigen autodiff igl::core igl_restricted::triangle RZFemBem mass_spring_utils
	COMPILE_DEFS ${GEOMETRY_NODES_COMPILE_DEFS}
	COMPILE_OPTIONS ${OpenMP_CXX_FLAGS}
)

# add OpenMP support to geometry_nodes target
target_link_options(geometry_nodes INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>)

target_link_libraries(node_points_to_mesh PRIVATE usd hioOpenVDB)

add_library(tetgen STATIC ${CMAKE_CURRENT_SOURCE_DIR}/tetgen/tetgen.cxx ${CMAKE_CURRENT_SOURCE_DIR}/tetgen/tetgen.h ${CMAKE_CURRENT_SOURCE_DIR}/tetgen/predicates.cxx)
set_target_properties(tetgen PROPERTIES ${OUTPUT_DIR})
if(MSVC)
	target_compile_options(tetgen PRIVATE /wd4477)
endif()
target_link_libraries(node_tetgen PRIVATE tetgen)
target_include_directories(node_tetgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tetgen)