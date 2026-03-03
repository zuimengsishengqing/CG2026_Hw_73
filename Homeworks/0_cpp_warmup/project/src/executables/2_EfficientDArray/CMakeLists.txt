# Simple CMakeLists.txt for 2_EfficientDArray
cmake_minimum_required(VERSION 3.27)
project(2_EfficientDArray)

# Add executable with specific source files
add_executable(2_EfficientDArray DArray.cpp main.cpp)

# Set output directory
set_target_properties(2_EfficientDArray PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)