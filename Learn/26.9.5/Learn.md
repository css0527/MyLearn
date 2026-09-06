# CMake
## 什么是 CMake？
    -CMake 是跨平台的构建工具生成器
    -它不直接编译代码，而是生成 Makefile 或 IDE 项目文件
    -在 ROS2 中，C++ 包使用 ament_cmake（基于 CMake）
    CMakeLists.txt → cmake → Makefile → make → 可执行文件


## CMake 基础语法
### 1. 最低版本要求
cmake_minimum_required(VERSION 3.10)

### 2. 项目名称
project(MyProject)

### 3. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

### 4. 添加可执行文件
add_executable(my_node src/main.cpp)

### 5. 链接库
target_link_libraries(my_node PUBLIC include)


## 创建 OpenCV 项目
mkdir cmake_opencv_demo
cd cmake_opencv_demo

#创建目录结构
mkdir -p src include build

#创建源文件
touch src/main.cpp
touch CMakeLists.txt


## 编写代码
#1. 基础设置
#最低 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

#项目名称
project(opencv_demo)

#设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#使用编译优化（可选）
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -O2)
endif()

#2. 查找依赖包

#查找 OpenCV（必需）
find_package(OpenCV REQUIRED)

#查找 OpenCV 的组件（可选）
#find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui)

#3. 设置头文件路径
#方式1：使用 include_directories（全局）
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

#方式2：使用 target_include_directories（推荐，更精确）
#target_include_directories(${PROJECT_NAME} PUBLIC
     ${CMAKE_CURRENT_SOURCE_DIR}/include
     ${OpenCV_INCLUDE_DIRS}
 )

#4. 添加可执行文件

#收集源文件
set(SOURCES
    src/main.cpp
)

#或者用 GLOB 自动收集（简单但不够精确）
#file(GLOB SOURCES "src/*.cpp")

#添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

#5. 链接库

#链接 OpenCV 库
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
)

#6. 设置输出目录（可选）

#设置可执行文件输出到 bin 目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#7. 打印信息（调试用）

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

#8. 安装规则（可选）

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)


#9. 测试支持（可选）

enable_testing()
add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME})


## 编译和运行
cd build

#配置（生成 Makefile）
cmake ..

#编译
make -j4

#准备一张测试图片（任意图片）
#或者创建一个简单的测试图片
#运行程序
./build/bin/opencv_demo test.jpg

#Or run
./main ../1.png


## CMake 进阶模板
### ROS2 C++ 包模板

#ROS2 C++ 包 CMakeLists.txt 模板
cmake_minimum_required(VERSION 3.8)
project(my_ros_cpp_pkg)

#使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#编译选项
add_compile_options(-Wall -Wextra -O3)

#查找 ROS2 依赖
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
#查找系统包
find_package(OpenCV REQUIRED)

#包含头文件
include_directories(
    include
    ${OpenCV_INCLUDE_DIRS}
)

#创建库
add_library(${PROJECT_NAME}_lib SHARED
    src/my_class.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(${PROJECT_NAME}_lib
    rclcpp
    std_msgs
    OpenCV
)

#创建可执行文件
add_executable(${PROJECT_NAME}_node
    src/main.cpp
)

target_include_directories(${PROJECT_NAME}_node PUBLIC
    include
    ${OpenCV_INCLUDE_DIRS}
)

ament_target_dependencies(${PROJECT_NAME}_node
    rclcpp
    std_msgs
    OpenCV
)

target_link_libraries(${PROJECT_NAME}_node
    ${PROJECT_NAME}_lib
    ${OpenCV_LIBS}
)

#注册为 ROS2 组件（可选）
rclcpp_components_register_node(${PROJECT_NAME}_node
    PLUGIN "my_pkg::MyNode"
    EXECUTABLE ${PROJECT_NAME}_node
)

#安装
install(TARGETS
    ${PROJECT_NAME}_node
    ${PROJECT_NAME}_lib
    EXPORT ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)

ament_export_targets(${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp std_msgs OpenCV)

ament_package()
