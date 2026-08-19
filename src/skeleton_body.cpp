#include "skeleton_body.h"

#include <algorithm>
#include <cmath>

namespace space3d {
namespace {
constexpr float Epsilon = 1.0e-5f;

float Clamp(float value, float low, float high) {
    return std::max(low, std::min(high, value));
}

float WrapAngle(float angle) {
    while (angle > Pi) angle -= 2.0f * Pi;
    while (angle < -Pi) angle += 2.0f * Pi;
    return angle;
}

Vector3 HorizontalUnit(const Vector3& value, const Vector3& fallback) {
    Vector3 flat{value.x, 0.0f, value.z};
    return flat.LengthSquared() > Epsilon ? flat.Normalized() : fallback;
}
} // namespace

HumanoidController::HumanoidController() {
    Reset();
}

void HumanoidController::Reset(const Vector3& ground_position) {
    state_ = {};
    state_.position = ground_position;
    state_.grounded = true;
}

void HumanoidController::Update(float elapsed_seconds, const HumanoidInput& input,
                                const Vector3& camera_forward, const Vector3& camera_right,
                                float ground_y, float half_extent) {
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0f) return;
    float remaining = std::min(elapsed_seconds, 0.25f);
    bool jump_available = input.jump;
    while (remaining > 0.0f) {
        const float dt = std::min(remaining, 1.0f / 120.0f);
        HumanoidInput step_input = input;
        step_input.jump = jump_available;
        SimulateStep(dt, step_input, camera_forward, camera_right, ground_y, half_extent);
        jump_available = false;
        remaining -= dt;
    }
}

void HumanoidController::SimulateStep(float dt, const HumanoidInput& input,
                                      const Vector3& camera_forward, const Vector3& camera_right,
                                      float ground_y, float half_extent) {
    const Vector3 forward = HorizontalUnit(camera_forward, {0.0f, 0.0f, -1.0f});
    Vector3 right = HorizontalUnit(camera_right, {-forward.z, 0.0f, forward.x});
    // Remove any residual non-orthogonal component while preserving the camera's handedness.
    right = (right - forward * right.Dot(forward)).Normalized();
    if (right.LengthSquared() <= Epsilon) right = {-forward.z, 0.0f, forward.x};

    const float input_forward = Clamp(input.forward, -1.0f, 1.0f);
    const float input_right = Clamp(input.right, -1.0f, 1.0f);
    Vector3 desired = forward * input_forward + right * input_right;
    const float input_length = std::min(1.0f, desired.Length());
    if (desired.LengthSquared() > Epsilon) desired = desired.Normalized();

    const float target_speed = input_length * (input.run ? RunSpeedMetersPerSecond : WalkSpeedMetersPerSecond);
    const float response = state_.grounded ? 12.0f : 3.0f;
    const float blend = 1.0f - std::exp(-response * dt);
    Vector3 horizontal{state_.velocity.x, 0.0f, state_.velocity.z};
    const Vector3 target_velocity = desired * target_speed;
    horizontal += (target_velocity - horizontal) * blend;
    state_.velocity.x = horizontal.x;
    state_.velocity.z = horizontal.z;

    if (desired.LengthSquared() > Epsilon) {
        const float desired_heading = std::atan2(desired.x, desired.z);
        const float turn = WrapAngle(desired_heading - state_.heading_radians);
        state_.heading_radians = WrapAngle(state_.heading_radians + turn * std::min(1.0f, 10.0f * dt));
    }

    if (input.jump && state_.grounded) {
        state_.velocity.y = JumpSpeedMetersPerSecond;
        state_.grounded = false;
    }
    if (!state_.grounded) state_.velocity.y -= 9.81f * dt;

    state_.position += state_.velocity * dt;
    if (state_.position.y <= ground_y) {
        state_.position.y = ground_y;
        if (state_.velocity.y < 0.0f) state_.velocity.y = 0.0f;
        state_.grounded = true;
    }

    const float limit = std::max(1.0f, half_extent) - 0.25f;
    if (state_.position.x > limit) { state_.position.x = limit; state_.velocity.x = 0.0f; }
    if (state_.position.x < -limit) { state_.position.x = -limit; state_.velocity.x = 0.0f; }
    if (state_.position.z > limit) { state_.position.z = limit; state_.velocity.z = 0.0f; }
    if (state_.position.z < -limit) { state_.position.z = -limit; state_.velocity.z = 0.0f; }

    state_.horizontal_speed_mps = std::sqrt(state_.velocity.x * state_.velocity.x + state_.velocity.z * state_.velocity.z);
    state_.running = input.run && state_.horizontal_speed_mps > WalkSpeedMetersPerSecond + 0.2f;
    if (state_.grounded && state_.horizontal_speed_mps > 0.08f) {
        const float stride = state_.running ? 1.45f : 1.15f;
        state_.gait_phase = std::fmod(state_.gait_phase + state_.horizontal_speed_mps * dt * (2.0f * Pi / stride), 2.0f * Pi);
    }
}

const HumanoidState& HumanoidController::GetState() const {
    return state_;
}

HumanoidPose HumanoidController::BuildPose() const {
    HumanoidPose pose;
    const Vector3 up{0.0f, 1.0f, 0.0f};
    const Vector3 forward{std::sin(state_.heading_radians), 0.0f, std::cos(state_.heading_radians)};
    const Vector3 right{forward.z, 0.0f, -forward.x};
    const Vector3 base = state_.position;
    auto point = [&](float x, float y, float z) { return base + right * x + up * y + forward * z; };
    auto set = [&](HumanoidJoint joint, const Vector3& value) { pose.joints[static_cast<std::size_t>(joint)] = value; };

    const float motion = state_.grounded ? Clamp(state_.horizontal_speed_mps / RunSpeedMetersPerSecond, 0.0f, 1.0f) : 0.0f;
    const float swing = std::sin(state_.gait_phase) * motion;
    const float left_lift = std::max(0.0f, std::sin(state_.gait_phase)) * 0.16f * motion;
    const float right_lift = std::max(0.0f, -std::sin(state_.gait_phase)) * 0.16f * motion;
    const float stride = 0.36f * swing;
    const float arm_swing = 0.28f * swing;
    const float bob = state_.grounded ? std::abs(std::sin(state_.gait_phase * 2.0f)) * 0.025f * motion : 0.0f;

    set(HumanoidJoint::Pelvis, point(0.0f, 0.95f + bob, 0.0f));
    set(HumanoidJoint::Spine, point(0.0f, 1.18f + bob, 0.0f));
    set(HumanoidJoint::Chest, point(0.0f, 1.40f + bob, 0.0f));
    set(HumanoidJoint::Neck, point(0.0f, 1.56f + bob, 0.0f));
    set(HumanoidJoint::Head, point(0.0f, HeightMeters + bob, 0.0f));

    set(HumanoidJoint::LeftShoulder, point(-0.22f, 1.46f + bob, 0.0f));
    set(HumanoidJoint::LeftElbow, point(-0.34f, 1.18f + bob, -arm_swing));
    set(HumanoidJoint::LeftHand, point(-0.34f, 0.90f + bob, -arm_swing * 1.45f));
    set(HumanoidJoint::RightShoulder, point(0.22f, 1.46f + bob, 0.0f));
    set(HumanoidJoint::RightElbow, point(0.34f, 1.18f + bob, arm_swing));
    set(HumanoidJoint::RightHand, point(0.34f, 0.90f + bob, arm_swing * 1.45f));

    set(HumanoidJoint::LeftHip, point(-0.12f, 0.94f + bob, 0.0f));
    set(HumanoidJoint::LeftKnee, point(-0.12f, 0.49f + left_lift * 0.55f, stride * 0.48f));
    set(HumanoidJoint::LeftFoot, point(-0.12f, 0.04f + left_lift, stride));
    set(HumanoidJoint::RightHip, point(0.12f, 0.94f + bob, 0.0f));
    set(HumanoidJoint::RightKnee, point(0.12f, 0.49f + right_lift * 0.55f, -stride * 0.48f));
    set(HumanoidJoint::RightFoot, point(0.12f, 0.04f + right_lift, -stride));
    return pose;
}

const std::array<std::pair<HumanoidJoint, HumanoidJoint>, 16>& HumanoidController::Bones() {
    static const std::array<std::pair<HumanoidJoint, HumanoidJoint>, 16> bones{{
        {HumanoidJoint::Pelvis, HumanoidJoint::Spine},
        {HumanoidJoint::Spine, HumanoidJoint::Chest},
        {HumanoidJoint::Chest, HumanoidJoint::Neck},
        {HumanoidJoint::Neck, HumanoidJoint::Head},
        {HumanoidJoint::Chest, HumanoidJoint::LeftShoulder},
        {HumanoidJoint::LeftShoulder, HumanoidJoint::LeftElbow},
        {HumanoidJoint::LeftElbow, HumanoidJoint::LeftHand},
        {HumanoidJoint::Chest, HumanoidJoint::RightShoulder},
        {HumanoidJoint::RightShoulder, HumanoidJoint::RightElbow},
        {HumanoidJoint::RightElbow, HumanoidJoint::RightHand},
        {HumanoidJoint::Pelvis, HumanoidJoint::LeftHip},
        {HumanoidJoint::LeftHip, HumanoidJoint::LeftKnee},
        {HumanoidJoint::LeftKnee, HumanoidJoint::LeftFoot},
        {HumanoidJoint::Pelvis, HumanoidJoint::RightHip},
        {HumanoidJoint::RightHip, HumanoidJoint::RightKnee},
        {HumanoidJoint::RightKnee, HumanoidJoint::RightFoot}
    }};
    return bones;
}

} // namespace space3d
