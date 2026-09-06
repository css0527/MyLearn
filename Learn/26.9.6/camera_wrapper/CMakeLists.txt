# ============================================
# CMakeLists.txt - CameraWrapper Demo
# ============================================

cmake_minimum_required(VERSION 3.10)
project(camera_wrapper_demo)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
add_compile_options(-Wall -Wextra -O2)

# 查找 OpenCV
find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found! Install: sudo apt install libopencv-dev")
endif()

# 包含头文件
include_directories(
    include
    ${OpenCV_INCLUDE_DIRS}
)

# ============================================
# 创建库（可选）
# ============================================

add_library(camera_wrapper SHARED
    src/CameraWrapper.cpp
)

target_include_directories(camera_wrapper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(camera_wrapper
    ${OpenCV_LIBS}
)

# ============================================
# 创建可执行文件
# ============================================

add_executable(main
    src/main.cpp
)

target_include_directories(main PUBLIC
    include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(main
    camera_wrapper
    ${OpenCV_LIBS}
)

# 设置输出目录
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ============================================
# 打印信息
# ============================================

message(STATUS "✅ OpenCV version: ${OpenCV_VERSION}")
message(STATUS "📁 Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "📁 Executable: ${CMAKE_BINARY_DIR}/main")
