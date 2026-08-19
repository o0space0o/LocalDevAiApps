#include "space_3d_core.h"
#include <iostream>
#include <sstream>

using namespace space3d;

int wmain(int argc, wchar_t** argv) {
    // Create a test scene with known balls
    Space3DEngine original;
    original.AddBall(Vector3{0.0f, 5.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}, 0.5f, 1.0f);
    original.AddBall(Vector3{2.0f, 5.0f, 0.0f}, Vector3{-1.0f, 0.0f, 0.0f}, 0.6f, 1.2f);
    original.AddBall(Vector3{-2.0f, 3.0f, 0.0f}, Vector3{0.0f, 2.0f, 0.0f}, 0.4f, 0.8f);

    // Serialize the scene
    std::string serialized = original.Serialize();
    if (serialized.empty()) {
        std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: serialization returned empty string\n";
        return 1;
    }

    std::wcout << L"Serialized scene: " << serialized.length() << L" bytes\n";

    // Deserialize into a new engine instance
    Space3DEngine restored;
    std::string error;
    if (!restored.Deserialize(serialized, error)) {
        std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: deserialization failed: " << error.c_str() << L"\n";
        return 1;
    }

    // Verify ball count matches
    const auto& orig_balls = original.GetBalls();
    const auto& rest_balls = restored.GetBalls();
    if (orig_balls.size() != rest_balls.size()) {
        std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: ball count mismatch: " << orig_balls.size()
                  << L" vs " << rest_balls.size() << L"\n";
        return 1;
    }

    // Verify ball properties match (with tolerance for floating point)
    const float epsilon = 0.0001f;
    for (size_t i = 0; i < orig_balls.size(); ++i) {
        const auto& ob = orig_balls[i];
        const auto& rb = rest_balls[i];

        // Check key properties
        if (std::abs(ob.position.x - rb.position.x) > epsilon ||
            std::abs(ob.position.y - rb.position.y) > epsilon ||
            std::abs(ob.position.z - rb.position.z) > epsilon) {
            std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: ball " << i << L" position mismatch\n";
            return 1;
        }

        if (std::abs(ob.velocity.x - rb.velocity.x) > epsilon ||
            std::abs(ob.velocity.y - rb.velocity.y) > epsilon ||
            std::abs(ob.velocity.z - rb.velocity.z) > epsilon) {
            std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: ball " << i << L" velocity mismatch\n";
            return 1;
        }

        if (std::abs(ob.radius - rb.radius) > epsilon) {
            std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: ball " << i << L" radius mismatch\n";
            return 1;
        }

        if (std::abs(ob.mass - rb.mass) > epsilon) {
            std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: ball " << i << L" mass mismatch\n";
            return 1;
        }
    }

    // Double-serialize and verify determinism
    std::string serialized_again = restored.Serialize();
    if (serialized != serialized_again) {
        std::wcerr << L"SPACE_3D_SCENE_VERIFY_FAIL: serialization not deterministic\n";
        return 1;
    }

    std::wcout << L"Scene round-trip verified: " << orig_balls.size() << L" balls preserved\n";
    std::wcout << L"SPACE_3D_SCENE_VERIFY_OK\n";
    return 0;
}
