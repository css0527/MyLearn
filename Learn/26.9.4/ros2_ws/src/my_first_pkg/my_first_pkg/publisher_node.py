#!/usr/bin/env python3
"""
最简单的 ROS2 发布者节点
每秒发布一次 "Hello ROS2!" 消息
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MinimalPublisher(Node):
    def __init__(self):
        super().__init__('minimal_publisher')
        
        # 创建发布者
        self.publisher = self.create_publisher(String, 'hello_topic', 10)
        
        # 创建定时器（每秒一次） 1.0:发布频率
        self.timer = self.create_timer(1.0, self.timer_callback)
        
        self.get_logger().info('🚀 Publisher started! Publishing to /hello_topic')

    def timer_callback(self):
        msg = String()
        msg.data = f'Hello ROS2! Time: {self.get_clock().now().to_msg().sec}'
        self.publisher.publish(msg)
        self.get_logger().info(f'📤 Published: {msg.data}')

def main(args=None):
    rclpy.init(args=args) #init ROS2
    node = MinimalPublisher() #node base Class
    try:
        rclpy.spin(node) # keep node run
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
