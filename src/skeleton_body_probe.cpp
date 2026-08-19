#include "skeleton_body.h"

#include <cmath>
#include <iostream>

namespace {
bool Near(float value, float expected, float tolerance) {
    return std::abs(value - expected) <= tolerance;
}

bool Require(bool condition, const char* message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}
}

int main() {
    using namespace space3d;
    constexpr float ground = -5.0f;
    constexpr float extent = 100.0f;
    constexpr float dt = 1.0f / 120.0f;
    const Vector3 camera_forward{0.0f, 0.0f, -1.0f};
    const Vector3 camera_right{1.0f, 0.0f, 0.0f};
    bool ok = true;

    HumanoidController body;
    HumanoidInput input;
    input.forward = 1.0f;
    for (int i = 0; i < 240; ++i) body.Update(dt, input, camera_forward, camera_right, ground, extent);
    const auto walk = body.GetState();
    ok &= Require(Near(walk.horizontal_speed_mps, HumanoidController::WalkSpeedMetersPerSecond, 0.03f), "walk speed is not 1.4 m/s");
    ok &= Require(walk.position.z < -2.2f, "forward command did not move along camera forward");
    ok &= Require(!walk.running, "walking was incorrectly reported as running");

    body.Reset({0.0f, ground, 0.0f});
    input = {};
    input.right = 1.0f;
    input.run = true;
    for (int i = 0; i < 180; ++i) body.Update(dt, input, camera_forward, camera_right, ground, extent);
    const auto run = body.GetState();
    ok &= Require(Near(run.horizontal_speed_mps, HumanoidController::RunSpeedMetersPerSecond, 0.04f), "run speed is not 3.5 m/s");
    ok &= Require(run.position.x > 4.0f, "right command did not move along camera right");
    ok &= Require(run.running, "running state was not reported");

    body.Reset({0.0f, ground, 0.0f});
    input = {};
    input.jump = true;
    body.Update(dt, input, camera_forward, camera_right, ground, extent);
    ok &= Require(!body.GetState().grounded && body.GetState().velocity.y > 4.8f, "jump did not launch upward");
    float highest = body.GetState().position.y;
    input.jump = false;
    for (int i = 0; i < 240; ++i) {
        body.Update(dt, input, camera_forward, camera_right, ground, extent);
        highest = std::max(highest, body.GetState().position.y);
    }
    ok &= Require(highest > ground + 1.1f, "jump did not reach a human-scale height");
    ok &= Require(body.GetState().grounded && Near(body.GetState().position.y, ground, 0.001f), "body did not land on the space ground");

    const HumanoidPose pose = body.BuildPose();
    const auto index = [](HumanoidJoint joint) { return static_cast<std::size_t>(joint); };
    const float rendered_height = pose.joints[index(HumanoidJoint::Head)].y - body.GetState().position.y;
    ok &= Require(Near(rendered_height, HumanoidController::HeightMeters, 0.03f), "pose is not 1.75 meters tall");
    ok &= Require(HumanoidController::Bones().size() == 16, "skeleton bone topology is incomplete");

    body.Reset({0.0f, ground, 0.0f});
    input = {};
    input.forward = 1.0f;
    body.Update(1.0f / 30.0f, input, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, ground, extent);
    ok &= Require(body.GetState().velocity.x > 0.0f, "camera-relative direction control failed");

    if (!ok) return 1;
    std::cout << "SKELETON_BODY_PROBE_OK walk=1.4mps run=3.5mps jump=landed bones=16 camera_relative=ok\n";
    return 0;
}
