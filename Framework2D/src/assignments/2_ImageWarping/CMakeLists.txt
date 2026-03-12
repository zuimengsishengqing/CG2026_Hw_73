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
# 使用 header-only 模式，无需编译安装
set(DLIB_DIR "${THIRD_PARTY_DIR}/dlib")
if(EXISTS "${DLIB_DIR}/dlib")
  message(STATUS "Dlib found at: ${DLIB_DIR}")
  target_include_directories(${PROJECT_NAME} PRIVATE "${DLIB_DIR}")
  # 添加 dlib 宏定义
  target_compile_definitions(${PROJECT_NAME} PRIVATE DLIB_USE_CUDA=0)
else()
  message(STATUS "Dlib not found at: ${DLIB_DIR}")
  message(STATUS "Please download dlib and extract to: ${DLIB_DIR}")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES 
  DEBUG_POSTFIX "_d"
  RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_DIR}") 
target_link_libraries(${PROJECT_NAME} PUBLIC common) 
target_compile_definitions(${PROJECT_NAME} PRIVATE -DDATA_PATH="${FRAMEWORK2D_DIR}/../Homeworks/2_image_warping/data")