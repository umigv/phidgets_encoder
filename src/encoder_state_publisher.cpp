#include "encoder_state_publisher.h"

#include <libphidget21/phidget21.h> // EPHIDGET_NOTATTACHED

#include <utility> // std::move

namespace umigv {

RobotCharacteristics&
RobotCharacteristics::with_frame(std::string frame) noexcept {
    frame_id = std::move(frame);

    return *this;
}

RobotCharacteristics&
RobotCharacteristics::with_serial_number(const int serial) noexcept {
    serial_number = serial;

    return *this;
}

RobotCharacteristics&
RobotCharacteristics::with_rads_per_tick(const f64 left_rads,
                                         const f64 right_rads) noexcept {
    rads_per_tick.first = left_rads;
    rads_per_tick.second = right_rads;

    return *this;
}

EncoderStatePublisher::EncoderStatePublisher(
    ros::Publisher publisher, RobotCharacteristics characteristics
) noexcept
: phidgets::Encoder{ }, frame_id_{ std::move(characteristics.frame_id) },
  state_{ }, publisher_{ std::move(publisher) },
  rads_per_tick_{ characteristics.rads_per_tick } {
    try_attach(characteristics.serial_number);
}

EncoderStatePublisher::~EncoderStatePublisher() {
    phidgets::Phidget::close();
}

void EncoderStatePublisher::publish_state(const ros::TimerEvent &event) {
    EncoderState next_state = poll_encoders();

    const sensor_msgs::JointState message = make_message(next_state);

    state_ = std::move(next_state);

    publisher_.publish(message);
}

void EncoderStatePublisher::try_attach(const int serial_number) {
    phidgets::Encoder::open(serial_number);

    const int result = phidgets::Phidget::waitForAttachment(6000);

    if (result != EPHIDGET_OK) {
        throw PhidgetsException{ "EncoderStatePublisher::try_attach", result };
    }

    ROS_INFO_STREAM("connected to '" << phidgets::Phidget::getDeviceName()
                    << "' with serial number "
                    << phidgets::Phidget::getDeviceSerialNumber());

    phidgets::Encoder::setEnabled(0, true);
    phidgets::Encoder::setEnabled(1, true);
}

EncoderStatePublisher::EncoderState EncoderStatePublisher::poll_encoders() {
    return EncoderState{ phidgets::Encoder::getPosition(0),
                         phidgets::Encoder::getPosition(1), ros::Time::now() };
}

sensor_msgs::JointState
EncoderStatePublisher::make_message(const EncoderState next_state) const {
    static u32 sequence_id = 0;

    sensor_msgs::JointState message;

    message.header.seq = sequence_id++;
    message.header.stamp = next_state.update_time;
    message.header.frame_id = frame_id_;

    message.name = { "wheel0", "wheel1" };

    const f64 left_position = static_cast<f64>(next_state.left_ticks)
                              * rads_per_tick_.first;
    const f64 right_position = static_cast<f64>(next_state.right_ticks)
                            * rads_per_tick_.second;

    message.position = { left_position, right_position };

    const i32 delta_left = next_state.left_ticks - state_.left_ticks;
    const i32 delta_right = next_state.right_ticks - state_.right_ticks;
    const f64 delta_time =
        (next_state.update_time - state_.update_time).toSec();

    const f64 left_velocity = static_cast<f64>(delta_left) / delta_time;
    const f64 right_velocity = static_cast<f64>(delta_right) / delta_time;

    message.velocity = { left_velocity, right_velocity };

    return message;
}

void EncoderStatePublisher::attachHandler() { }

void EncoderStatePublisher::detachHandler() {
    throw PhidgetsException{ "PhidgetsWheelsPublisher::detachHandler",
                             EPHIDGET_NOTATTACHED };
}

void EncoderStatePublisher::errorHandler(const int error_code) {
    throw PhidgetsException{ "PhidgetsWheelsPublisher::errorHandler",
                             error_code };
}

void EncoderStatePublisher::indexHandler(int, int) { }

void EncoderStatePublisher::positionChangeHandler(int, int, int) { }

} // namespace umigv
