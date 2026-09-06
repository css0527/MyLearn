#!/usr/bin/env python3
"""
最简单的 ROS2 订阅者节点
接收并打印 /hello_topic 的消息
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MinimalSubscriber(Node):
    def __init__(self):
        super().__init__('minimal_subscriber')
        
        # 创建订阅者
        self.subscription = self.create_subscription(
            String,
            'hello_topic',
            self.listener_callback,
            10
        )
        
        self.get_logger().info('👂 Subscriber started! Listening to /hello_topic')

    def listener_callback(self, msg):
        self.get_logger().info(f'📥 Received: {msg.data}')

def main(args=None):
    rclpy.init(args=args)
    node = MinimalSubscriber()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
