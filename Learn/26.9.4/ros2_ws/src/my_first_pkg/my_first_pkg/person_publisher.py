#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from custom_msgs.msg import Person

class PersonPublisher(Node):
    def __init__(self):
        super().__init__('person_publisher')
        self.publisher = self.create_publisher(Person, 'person_topic', 10)
        self.timer = self.create_timer(2.0, self.timer_callback)
        self.get_logger().info('Person Publisher started!')

        # 在 __init__ 中添加
        self.declare_parameter('publish_frequency', 1.0)
        freq = self.get_parameter('publish_frequency').value
        self.timer = self.create_timer(freq, self.timer_callback)

    def timer_callback(self):
        msg = Person()
        msg.name = 'ROS2 Learner'
        msg.age = 25
        msg.height = 1.75
        self.publisher.publish(msg)
        self.get_logger().info(f'Published: {msg.name}, {msg.age}, {msg.height}')

def main(args=None):
    rclpy.init(args=args)
    node = PersonPublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
