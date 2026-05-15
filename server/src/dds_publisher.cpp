#include "joystick_server/dds_publisher.h"

#include <dds/dds.hpp>

#include <iostream>

#include "JoystickData.hpp"

namespace joystick_server {

struct DdsPublisher::Impl {
  dds::domain::DomainParticipant participant;
  dds::topic::Topic<joy_data::JoyData> topic;
  dds::pub::Publisher publisher;
  dds::pub::DataWriter<joy_data::JoyData> writer;
  std::string frame_id;

  Impl(const JoystickConfig& config)
    : participant(org::eclipse::cyclonedds::domain::default_id())
    , topic(participant, config.dds_topic)
    , publisher(participant)
    , writer(publisher, topic)
    , frame_id(config.frame_id)
  {}
};

DdsPublisher::DdsPublisher() = default;
DdsPublisher::~DdsPublisher() = default;

bool DdsPublisher::init(const JoystickConfig& config) {
  try {
    impl_ = std::make_unique<Impl>(config);
    std::cout << "DdsPublisher: publishing on topic \"" << config.dds_topic
              << "\"" << std::endl;
  } catch (const dds::core::Exception& e) {
    std::cerr << "DdsPublisher: init failed: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void DdsPublisher::publish(const JoystickData& data) {
  if (!impl_) return;

  joy_data::JoyData sample;

  sample.axes().assign(data.axes.begin(), data.axes.end());
  sample.buttons().assign(data.buttons.begin(), data.buttons.end());
  sample.timestamp_ns() = data.timestamp_ns;
  sample.frame_id() = impl_->frame_id;

  impl_->writer.write(sample);
}

}  // namespace joystick_server
