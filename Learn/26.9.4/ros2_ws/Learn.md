# 创建学习工作空间
## 1. 创建学习目录
mkdir -p /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/src
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws

## 2. 设置 ROS2 环境（每次开新终端都需要）
source /opt/ros/humble/setup.bash

## 3. 创建功能包
cd src
ros2 pkg create my_first_pkg \
  --build-type ament_python \
  --dependencies rclpy std_msgs

## 4. 查看创建的文件
tree my_first_pkg/


# 创建发布者节点
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/src/my_first_pkg/my_first_pkg/

#创建发布者文件
touch publisher_node.py
chmod +x publisher_node.py

#用 nano 编辑（或 gedit）
nano publisher_node.py

# 创建订阅者节点
#创建订阅者文件
touch subscriber_node.py
chmod +x subscriber_node.py

nano subscriber_node.py

# 配置 setup.py
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/src/my_first_pkg/
nano setup.py
找到 entry_points 部分，修改为：
entry_points={
    'console_scripts': [
        'publisher = my_first_pkg.publisher_node:main',
        'subscriber = my_first_pkg.subscriber_node:main',
    ],
},

# 编译
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws

#确保 ROS2 环境已 source
source /opt/ros/humble/setup.bash

#编译
colcon build --packages-select my_first_pkg --symlink-install

## 如果编译成功，会看到：
Starting >>> my_first_pkg
Finished <<< my_first_pkg [0.5s]

# 运行测试
## 打开三个终端
### 终端 1（发布者）：
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run my_first_pkg publisher

### 终端 2（订阅者）：
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run my_first_pkg subscriber

### 终端 3（调试）：
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash

#查看话题
ros2 topic list

#查看话题消息
ros2 topic echo /hello_topic

#查看节点
ros2 node list

# 为了方便，添加到 .bashrc
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
echo "source /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/install/setup.bash" >> ~/.bashrc
source ~/.bashrc

这样每次打开终端都会自动加载 ROS2 环境。

# ROS2 Tools
#1. 查看所有节点
ros2 node list

#2. 查看节点信息
ros2 node info /minimal_publisher

#3. 查看所有话题
ros2 topic list

#4. 查看话题信息
ros2 topic info /hello_topic

#5. 查看话题消息类型
ros2 topic type /hello_topic

#6. 查看话题数据（实时）
ros2 topic echo /hello_topic

#7. 查看话题发布频率
ros2 topic hz /hello_topic

#8. 手动发布消息（测试订阅者）
ros2 topic pub /hello_topic std_msgs/msg/String "data: 'Hello from CLI!'"

#9. 查看服务列表
ros2 service list

#10. 查看参数
ros2 param list

# 添加自定义消息
## 创建自定义消息：
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/src
ros2 pkg create custom_msgs --build-type ament_cmake
cd custom_msgs
mkdir msg

## 创建消息文件：
nano msg/Person.msg

## 修改 CMakeLists.txt：
find_package(rosidl_default_generators REQUIRED)
#在文件末尾添加
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Person.msg"
)

ament_export_dependencies(rosidl_default_runtime)

## 修改 package.xml：
<build_depend>rosidl_default_generators</build_depend>
<exec_depend>rosidl_default_runtime</exec_depend>
<member_of_group>rosidl_interface_packages</member_of_group>

# 使用自定义消息
## 创建一个使用 Person.msg 的新节点：
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/src/my_first_pkg/my_first_pkg/
touch person_publisher.py
chmod +x person_publisher.py
nano person_publisher.py

# 添加参数
#在 __init__ 中添加
self.declare_parameter('publish_frequency', 1.0)
freq = self.get_parameter('publish_frequency').value
self.timer = self.create_timer(freq, self.timer_callback)

## 运行时修改参数：
ros2 run my_first_pkg publisher --ros-args -p publish_frequency:=0.5

# 创建 Launch 文件
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws/src/my_first_pkg
mkdir launch
cd launch
touch bringup.launch.py
chmod +x bringup.launch.py
nano bringup.launch.py

## 修改 setup.py，添加 launch 文件：
import os
from glob import glob

data_files=[
    ('share/ament_index/resource_index/packages',
        ['resource/' + package_name]),
    ('share/' + package_name, ['package.xml']),
    (os.path.join('share', package_name), glob('launch/*.launch.py')),
],
## 重新编译并运行：
cd /home/ubuntu22/MyLearn/Learn/26.9.4/ros2_ws
#清理旧的编译
rm -rf build/ install/ log/
#重新编译
colcon build --packages-select my_first_pkg
(colcon build --packages-select custom_msgs)
source install/setup.bash
ros2 launch my_first_pkg bringup.launch.py

# 调试技巧
## 1. 查看日志
#查看所有日志
ros2 bag record -a

#查看特定话题日志
ros2 bag record /hello_topic

## 2. 可视化
#安装 rqt（如果没有）
sudo apt install ros-humble-rqt*

#启动 rqt
rqt

#启动 rqt_graph（查看节点关系）
rqt_graph

#启动 rqt_plot（绘制数据曲线）
rqt_plot

#启动 rqt_console（查看日志）
rqt_console

## 3. 调试打印
# 使用不同级别的日志
self.get_logger().debug('Debug message')
self.get_logger().info('Info message')
self.get_logger().warn('Warning message')
self.get_logger().error('Error message')
self.get_logger().fatal('Fatal message')

# 理解项目(QD_Vision2026) 
#查看 armor_detector 节点（它和你的 publisher 类似）
cat rm_auto_aim/armor_detector/src/armor_detector_node.cpp | head -50

#查看 armor_solver 节点
cat rm_auto_aim/armor_solver/src/armor_solver_node.cpp | head -50

#查看话题通信
ros2 topic list
ros2 topic echo /armor_detector/armors --once
