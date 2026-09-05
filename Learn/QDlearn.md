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
# 1. 图像和内参是否在发（应该是 ~100Hz，Hz 显示为 0 就说明没发）
ros2 topic hz /image_raw
ros2 topic hz /camera_info

# 2. 内参 k 是否非零（非零=标定加载成功）
ros2 topic echo /camera_info --once | grep -A2 "k:"

# 3. 检测话题是否在发（即使是空 Armors 也会发 header）
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
    
