#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#include "space_3d_core.h"

namespace {
int Fail(const char* message) {
    std::cerr << "SPACE3D_ADVANCED_PROBE_FAIL: " << message << '\n';
    return 1;
}

bool Near(float a, float b, float tolerance = 0.02f) {
    return std::fabs(a - b) <= tolerance;
}
} // namespace

int main() {
    using namespace space3d;

    // Controlled elastic collision with gravity and drag disabled.
    Space3DEngine engine;
    engine.Clear();
    auto& settings = engine.GetSettings();
    settings.gravity = {0.0f, 0.0f, 0.0f};
    settings.linear_drag = 0.0f;
    settings.restitution = 1.0f;
    settings.ground_y = -50.0f;
    settings.half_extent = 50.0f;
    settings.fixed_step = 1.0f / 120.0f;
    settings.collisions_enabled = true;

    const std::uint32_t left_id = engine.AddBall({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, 0.5f, 1.0f);
    const std::uint32_t right_id = engine.AddBall({2.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}, 0.5f, 1.0f);
    if (left_id == 0 || right_id == 0 || engine.GetBalls().size() != 2) return Fail("object creation failed");
    if (engine.AddBall({}, {}, -1.0f, 1.0f) != 0) return Fail("invalid object was accepted");

    for (int i = 0; i < 240; ++i) engine.Update(1.0f / 240.0f);
    const Ball* left = engine.FindBall(left_id);
    const Ball* right = engine.FindBall(right_id);
    if (!left || !right) return Fail("object lookup failed");
    if (!(left->velocity.x < -1.8f && right->velocity.x > 1.8f)) return Fail("elastic collision response failed");

    const Diagnostics collision_diagnostics = engine.GetDiagnostics();
    if (collision_diagnostics.simulation_steps < 119 || collision_diagnostics.collision_count == 0 ||
        collision_diagnostics.total_kinetic_energy < 3.5f) {
        return Fail("physics diagnostics are implausible");
    }

    const float old_speed = left->velocity.Length();
    if (!engine.ApplyImpulse(left_id, {0.0f, 3.0f, 0.0f})) return Fail("impulse tool failed");
    left = engine.FindBall(left_id);
    if (!left || left->velocity.Length() <= old_speed) return Fail("impulse did not change velocity");
    if (engine.GetBallStats(left_id).find(L"Ball #") == std::wstring::npos) return Fail("inspector text failed");

    // Camera projection and picking: aim at the selected ball and raycast through screen center.
    Camera& camera = engine.GetCamera();
    camera.target = left->position;
    camera.yaw = 0.7f;
    camera.pitch = 0.25f;
    camera.distance = 12.0f;
    const ScreenPoint projected = camera.Project(left->position, 1280.0f, 720.0f);
    if (!projected.visible || !Near(projected.x, 640.0f, 0.5f) || !Near(projected.y, 360.0f, 0.5f)) {
        return Fail("camera projection failed");
    }
    const RayHit hit = engine.Raycast(camera.MakeRay(640.0f, 360.0f, 1280.0f, 720.0f));
    if (hit.ball_id != left_id || hit.distance <= 0.0f) return Fail("camera picking ray failed");

    // Versioned reusable document round-trip.
    const std::string document = engine.Serialize();
    if (document.rfind("SPACE3D 1\n", 0) != 0) return Fail("versioned serialization header missing");
    Space3DEngine restored;
    std::string error;
    if (!restored.Deserialize(document, error) || restored.GetBalls().size() != 2) return Fail("serialization round-trip failed");
    const Ball* restored_left = restored.FindBall(left_id);
    if (!restored_left || !Near(restored_left->mass, left->mass) || !Near(restored_left->position.x, left->position.x)) {
        return Fail("round-trip changed object data");
    }
    if (restored.Deserialize("SPACE3D 99\n", error) || error.empty()) return Fail("invalid document was accepted");

    // Lifecycle controls: pause blocks elapsed updates, while StepOnce is deterministic.
    restored.SetPaused(true);
    const auto steps_before_pause = restored.GetDiagnostics().simulation_steps;
    restored.Update(0.1f);
    if (restored.GetDiagnostics().simulation_steps != steps_before_pause) return Fail("pause lifecycle rule failed");
    restored.StepOnce();
    if (restored.GetDiagnostics().simulation_steps != steps_before_pause + 1) return Fail("single-step tool failed");

    restored.Reset(Preset::Fountain);
    if (restored.GetBalls().size() != 12) return Fail("fountain preset failed");
    restored.Reset(Preset::DropTower);
    if (restored.GetBalls().size() != 8) return Fail("drop-tower preset failed");
    const std::uint32_t removable = restored.GetBalls().front().id;
    if (!restored.RemoveBall(removable) || restored.FindBall(removable) != nullptr || restored.GetBalls().size() != 7) {
        return Fail("remove tool failed");
    }

    std::cout << "SPACE3D_ADVANCED_PROBE_OK\n";
    return 0;
}
