project(2_ImageWarping)
file(GLOB source
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h" 
  "${CMAKE_CURRENT_SOURCE_DIR}/warper/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/warper/*.h" 
)
add_executable(${PROJECT_NAME} ${source})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# HW2_TODO(optional): Add Dlib support to the project
add_definitions(-D_HAS_STD_BYTE=0)
find_package(dlib QUIET)
if(dlib_FOUND)
  message(STATUS "Dlib found")
  target_link_libraries(${PROJECT_NAME} PRIVATE dlib::dlib)
else()
  message(STATUS "Dlib not found")
endif()

# Add Annoy support (header-only library)
if(EXISTS "${THIRD_PARTY_DIR}/annoy/src/annoylib.h")
  message(STATUS "Annoy found")
  target_include_directories(${PROJECT_NAME} PRIVATE "${THIRD_PARTY_DIR}/annoy/src")
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ANNOY)
else()
  message(STATUS "Annoy not found")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES 
  DEBUG_POSTFIX "_d"
  RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_DIR}") 
target_link_libraries(${PROJECT_NAME} PUBLIC common) 
target_compile_definitions(${PROJECT_NAME} PRIVATE -DDATA_PATH="${FRAMEWORK2D_DIR}/../Homeworks/2_image_warping/data")
# 解决 Windows 下 std::byte 与 rpcndr.h 中 byte 的冲突
target_compile_definitions(${PROJECT_NAME} PRIVATE _HAS_STD_BYTE=0)