cmake_minimum_required(VERSION 3.15)
project(VulkanComputeExample LANGUAGES CXX)

# C++20 is strictly required for the designated initializers used in your code
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Require the Vulkan SDK to be installed on the system
find_package(Vulkan REQUIRED)

# Define the executable
add_executable(VulkanCompute main.cpp)

# Link the Vulkan library to the executable
target_link_libraries(VulkanCompute PRIVATE Vulkan::Vulkan)

# Automatically compile the GLSL shader to SPIR-V during the build process
find_program(GLSLC_EXECUTABLE glslc HINTS $ENV{VULKAN_SDK}/bin)
if(GLSLC_EXECUTABLE)
    add_custom_command(
        TARGET VulkanCompute POST_BUILD
        COMMAND ${GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/shader.comp -o $<TARGET_FILE_DIR:VulkanCompute>/shader.spv
        COMMENT "Compiling shader.comp to shader.spv using glslc"
        VERBATIM
    )
else()
    message(WARNING "glslc compiler not found. You will need to compile shader.comp to shader.spv manually.")
endif()