#include "joystick_server/ros2_publisher.h"

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joy.hpp>

#include <memory>

namespace joystick_server {

struct Ros2Publisher::Impl {
  std::shared_ptr<rclcpp::Node> node;
  rclcpp::Publisher<sensor_msgs::msg::Joy>::SharedPtr pub;
  rclcpp::executors::SingleThreadedExecutor::SharedPtr executor;
  std::string frame_id;

  Impl() {
    node = std::make_shared<rclcpp::Node>("joystick_server");
    executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor->add_node(node);
  }
};

Ros2Publisher::Ros2Publisher() = default;
Ros2Publisher::~Ros2Publisher() = default;

bool Ros2Publisher::init(const JoystickConfig& config) {
  impl_ = std::make_unique<Impl>();
  impl_->frame_id = config.frame_id;
  impl_->pub = impl_->node->create_publisher<sensor_msgs::msg::Joy>(config.ros2_topic, 10);
  std::cout << "Ros2Publisher: publishing on topic \"" << config.ros2_topic << "\""
            << " (node: " << impl_->node->get_name() << ")" << std::endl;
  return true;
}

void Ros2Publisher::publish(const JoystickData& data) {
  if (!impl_ || !impl_->pub) return;

  auto msg = std::make_unique<sensor_msgs::msg::Joy>();
  msg->header.stamp = rclcpp::Time(data.timestamp_ns, RCL_STEADY_TIME);
  msg->header.frame_id = impl_->frame_id;
  msg->axes = data.axes;
  msg->buttons = data.buttons;

  impl_->pub->publish(std::move(msg));
}

void Ros2Publisher::spinOnce() {
  if (impl_ && impl_->executor) {
    impl_->executor->spin_some();
  }
}

}  // namespace joystick_server
