cmake_minimum_required(VERSION 3.10)
project(opencv_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 添加可执行文件
add_executable(main src/main.cpp)   # 注意：这里生成的可执行文件名是 main

# 链接 OpenCV
target_link_libraries(main ${OpenCV_LIBS})

# 包含头文件
target_include_directories(main PRIVATE ${OpenCV_INCLUDE_DIRS})

