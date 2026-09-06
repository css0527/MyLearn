## 一、 error : terminate called after throwing an instance of 'std::runtime_error' what():  can't compare times with different time sources
  
Bug 1：时间源不匹配（崩溃的直接原因）
vision_state_machine.cpp:421 里那段"防护性修正"代码：


rclcpp::Time image_time(
    std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch()).count()  // 默认 SYSTEM_TIME
);
...
rclcpp::Time now = node_->now();   // ROS_TIME
if (image_time > now + kMaxFutureOffset) { ... }   // 两种时间源比较 → 抛异常
rclcpp::Time(int64_t) 构造出来的是 SYSTEM_TIME
node_->now() 是 ROS_TIME
两者一比较就抛 can't compare times with different time sources
这个异常在相机驱动的 captureLoop 线程里没有被 catch，直接 std::terminate 杀掉了整个 component_container_mt。

修复：让 image_time 用和节点一致的时间源：


rclcpp::Time image_time(
    std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch()).count(),
    node_->get_clock()->get_clock_type()   // 与 node_->now() 同源
);

## 二、调试查看话题
#1. 图像和内参是否在发（应该是 ~100Hz，Hz 显示为 0 就说明没发）
ros2 topic hz /image_raw
ros2 topic hz /camera_info

#2. 内参 k 是否非零（非零=标定加载成功）
ros2 topic echo /camera_info --once | grep -A2 "k:"

#3. 检测话题是否在发（即使是空 Armors 也会发 header）
ros2 topic hz /armor_detector/armors

## 三、result_img 没画面	代码里根本没有 result_img 发布器
armor_detector_node.hpp:
    image_transport::Publisher binary_img_pub_;
    image_transport::Publisher number_img_pub_;
    image_transport::Publisher result_img_pub_;
    
armor_detector_node.cpp:
void ArmorDetectorNode::createDebugPublishers() 
    binary_img_pub_ = image_transport::create_publisher(this, "armor_detector/binary_img");
    number_img_pub_ = image_transport::create_publisher(this, "armor_detector/number_img");
    result_img_pub_ = image_transport::create_publisher(this, "armor_detector/result_img");
    
void ArmorDetectorNode::destroyDebugPublishers()
    binary_img_pub_.shutdown();
    number_img_pub_.shutdown();
    result_img_pub_.shutdown();
    
    
## 四、整幅画面偏蓝/红
armor_detector_node.cpp :
  ArmorDetectorNode::detectArmors(cv::Mat& img, const std_msgs::msg::Header& header) 
  if (debug_) {
  	// 发布标注后的结果图（检测框/灯条/数字），供 rqt_image_view / foxglove 可视化
        //rqt 拿到 bgr8 标签后，把 RGB 数据按 BGR 去解读，红蓝通道就互换了——所以红色灯条显示成蓝色、整幅画面偏蓝。
        // 注意：编码需与 image_raw 一致（rgb8），否则红蓝通道会互换。
        result_img_pub_.publish(cv_bridge::CvImage(header, "rgb8", img).toImageMsg());
       }
          
 我上一版发布 result_img 时编码写成了 "bgr8"，但你的图像数据实际是 RGB（跟 image_raw 的 "rgb8" 标签一致）。rqt 拿到 bgr8 标签后，把 RGB 数据按 BGR 去解读，红蓝通道就互换了——所以红色灯条显示成蓝色、整幅画面偏蓝。

binary_img 是灰度图没有颜色问题，image_raw 用的是 rgb8 标签所以正常，只有我新加的 result_img 写错了标签。
    
## 五、可视化
ros2 run rqt_image_view rqt_image_view

## Ctrl+Z 和 Ctrl+C
操作	 效果
Ctrl+Z	只是暂停进程，进程还活着、还占着相机 → 下次启动 MV_E_ACCESS_DENIED
Ctrl+C	正常退出，走析构释放相机 → 下次能直接打开

如果 Ctrl+C 卡住没退干净（偶发），用这个兜底彻底清：

pkill -9 -f component_container_mt
pkill -9 -f bringup_SingleProcess

可以做成一个别名，比如 
alias killros='pkill -9 -f component_container_mt; pkill -9 -f bringup_SingleProcess'

## 路径问题
因为用的是 --symlink-install，launch 文件是指向源码的软链接，所以不用重新 build，直接重新 ros2 launch 即可。

改动汇总（都是把硬编码的旧路径 /home/scurm/QD_Vision2026 换成从当前位置推导）：

bringup_SingleProcess.launch.py — 用 get_package_share_directory('rm_bringup') 向上 4 级算出工作空间根目录，再拼 .pixi/... 和 src/rm_utils/hikSDK/...。
start_vision.sh 和 launch_complete.sh — 用 SCRIPT_DIR（脚本所在目录）代替 ~/QD_Vision2026 和绝对路径。
docker-compose.yaml — 挂载源改成自己实际路径 /home/scurm/css/QD_Vision2026:/ros_ws。

docker-compose 那条挂载，如果平时是在容器里跑（/ros_ws），确认一下容器里对应路径也要跟着改；如果纯宿主机跑，这条不影响。
当前的 shell 是 (rmvision) conda 环境，而 launch 里 LD_PRELOAD 指向的是 pixi 的 libstdc++.so.6。

## 画面延迟高
带宽问题：图像以未压缩的原始格式、全分辨率、100Hz 在发，可视化根本跟不上。

证据链
分辨率 camera_info.yaml：1280×1024
帧率 camera_driver_params.yaml:11：acquisition_frame_rate: 100.0
发布编码 hik_camera_node.cpp:112：RGB8 原始图，而且 第36行 是普通 create_publisher（没有 /image_raw/compressed）
result_img 也是 RGB8 原始图（走 image_transport，但容器没加载 compressed 插件的话也只有 raw）
单帧 = 1280×1024×3 ≈ 3.75 MB，100 Hz ≈ 390 MB/s。rviz/rqt_image_view 订阅这种 raw 话题，渲染线程根本消化不了，队列越堆越长，看到的画面就越来越滞后——两个话题都走 raw，所以两个都延迟。

（getCurrentImage 那边是 keep-latest、没有 FIFO 积压，所以延迟不在相机采集侧。）

怎么修（按性价比排序）
1. 调试期先把帧率降下来（最快，改一行）

camera_driver_params.yaml:11 100.0 → 30.0。带宽直接砍 3 倍多，画面立刻跟手。等真正调完再改回 100。

2. 看压缩话题

result_img 走的是 image_transport，如果容器加载了 compressed_image_transport 插件，可以在 rviz 里订阅 /armor_solver/result_img/compressed（压缩后带宽降 10~20 倍）。
image_raw 是普通 publisher，没有 compressed 变体，想看压缩版需要加一个 image_transport republish 节点，或把相机节点改成 image_transport::create_publisher。
3. YOLO 用 GPU（如果机器有核显/独显）

armor_detector_params.yaml:8 现在是 device: "CPU"，OpenVINO 跑 1280×1024 会很吃力，这会让 result_img 的更新率本身也低。有 iGPU 的话可以试 "GPU"，但这只影响检测帧率，不影响 image_raw 的延迟。

## GPU 和 CPU
机器上没有 OpenVINO 能用的 GPU（intel_gpu 插件查不到任何设备），所以 device: "GPU" 直接导致 armor_detector 构造时抛异常、节点加载失败。我把它改回 "CPU"。

配置文件是软链接，直接重新 launch 就行，不用 build。

机器上 OpenVINO 的 intel_gpu 插件查不到任何可用 GPU 设备（要么没有 Intel 核显，要么没装对应的 GPU 驱动），所以 "GPU" 会直接让 armor_detector 构造失败、节点加载不进来。以后想用 GPU 的话得先解决驱动，否则就保持 CPU。

1. CPU 下 YOLO 1280×1024 会比较慢，result_img 更新率会偏低——这是之前"延迟高"的一部分原因。要缓解可以：
把 camera_6mm.acquisition_frame_rate 降到 30~60（调试期足够，也顺带解决 raw 带宽问题）；
或者给 YOLO 换更小的输入尺寸 / 量化模型（这块要看模型文件）。
2. exit code -11（段错误）每次都稳定复现，发生在 HikCameraNode destroyed! 之后、进程退出阶段。这是 shutdown 时的析构 bug（很可能是相机节点/状态机里某个对象被析构两次，或 capture_thread_ 还在用已释放的资源）。不影响运行，但建议尽早查，否则每次退出都崩。
