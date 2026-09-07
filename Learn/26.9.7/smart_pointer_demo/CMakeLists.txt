cmake_minimum_required(VERSION 3.10)
project(smart_pointer_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -O2)

find_package(OpenCV REQUIRED)

include_directories(include ${OpenCV_INCLUDE_DIRS})

# 库
add_library(camera_wrapper SHARED src/CameraWrapper.cpp)
target_include_directories(camera_wrapper PUBLIC include)
target_link_libraries(camera_wrapper ${OpenCV_LIBS})

# 可执行文件
add_executable(main src/main.cpp)
target_include_directories(main PUBLIC include)
target_link_libraries(main camera_wrapper ${OpenCV_LIBS})

set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

message(STATUS "✅ OpenCV: ${OpenCV_VERSION}")
