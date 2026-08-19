#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "space_3d_core.h"

namespace space3d {

enum class HumanoidJoint : std::size_t {
    Pelvis,
    Spine,
    Chest,
    Neck,
    Head,
    LeftShoulder,
    LeftElbow,
    LeftHand,
    RightShoulder,
    RightElbow,
    RightHand,
    LeftHip,
    LeftKnee,
    LeftFoot,
    RightHip,
    RightKnee,
    RightFoot,
    Count
};

constexpr std::size_t HumanoidJointCount = static_cast<std::size_t>(HumanoidJoint::Count);

struct HumanoidInput {
    float forward = 0.0f;
    float right = 0.0f;
    bool run = false;
    bool jump = false;
};

struct HumanoidState {
    // Position is the point directly below the pelvis, at foot/ground level, in meters.
    Vector3 position{0.0f, -5.0f, 0.0f};
    Vector3 velocity;
    float heading_radians = 0.0f;
    float horizontal_speed_mps = 0.0f;
    float gait_phase = 0.0f;
    bool grounded = true;
    bool running = false;
};

struct HumanoidPose {
    std::array<Vector3, HumanoidJointCount> joints{};
};

class HumanoidController {
public:
    static constexpr float WalkSpeedMetersPerSecond = 1.4f;
    static constexpr float RunSpeedMetersPerSecond = 3.5f;
    static constexpr float JumpSpeedMetersPerSecond = 5.0f;
    static constexpr float HeightMeters = 1.75f;

    HumanoidController();

    void Reset(const Vector3& ground_position = {0.0f, -5.0f, 0.0f});
    void Update(float elapsed_seconds, const HumanoidInput& input,
                const Vector3& camera_forward, const Vector3& camera_right,
                float ground_y, float half_extent);

    const HumanoidState& GetState() const;
    HumanoidPose BuildPose() const;

    static const std::array<std::pair<HumanoidJoint, HumanoidJoint>, 16>& Bones();

private:
    void SimulateStep(float dt, const HumanoidInput& input,
                      const Vector3& camera_forward, const Vector3& camera_right,
                      float ground_y, float half_extent);

    HumanoidState state_;
};

} // namespace space3d
