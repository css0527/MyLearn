from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='my_first_pkg',
            executable='publisher',
            name='publisher_node',
            output='screen'
        ),
        Node(
            package='my_first_pkg',
            executable='subscriber',
            name='subscriber_node',
            output='screen'
        )
    ])
