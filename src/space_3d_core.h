#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace space3d {

constexpr float Pi = 3.14159265358979323846f;

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator+(const Vector3& value) const;
    Vector3 operator-(const Vector3& value) const;
    Vector3 operator-() const;
    Vector3 operator*(float scalar) const;
    Vector3 operator/(float scalar) const;
    Vector3& operator+=(const Vector3& value);
    Vector3& operator-=(const Vector3& value);
    Vector3& operator*=(float scalar);
    float Dot(const Vector3& value) const;
    Vector3 Cross(const Vector3& value) const;
    float LengthSquared() const;
    float Length() const;
    Vector3 Normalized() const;
};

struct Ray {
    Vector3 origin;
    Vector3 direction{0.0f, 0.0f, -1.0f};
};

struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    bool visible = false;
};

struct Camera {
    Vector3 target{0.0f, 1.0f, 0.0f};
    float yaw = 0.65f;
    float pitch = 0.35f;
    float distance = 24.0f;
    float vertical_fov_radians = 55.0f * Pi / 180.0f;
    float near_plane = 0.1f;

    Vector3 Position() const;
    Vector3 Forward() const;
    Vector3 Right() const;
    Vector3 Up() const;
    void Orbit(float yaw_delta, float pitch_delta);
    void Zoom(float amount);
    ScreenPoint Project(const Vector3& world, float width, float height) const;
    Ray MakeRay(float screen_x, float screen_y, float width, float height) const;
};

struct SimulationSettings {
    Vector3 gravity{0.0f, -9.81f, 0.0f};
    float restitution = 0.78f;
    float linear_drag = 0.12f;
    float ground_friction = 0.82f;
    float ground_y = -5.0f;
    float half_extent = 15.0f;
    float fixed_step = 1.0f / 120.0f;
    bool collisions_enabled = true;
};

struct Ball {
    std::uint32_t id = 0;
    Vector3 position;
    Vector3 velocity;
    float radius = 0.5f;
    float mass = 1.0f;

    float Volume() const;
    float Density() const;
    float KineticEnergy() const;
};

struct RayHit {
    std::uint32_t ball_id = 0;
    float distance = 0.0f;
    Vector3 point;
};

struct Diagnostics {
    std::size_t ball_count = 0;
    std::uint64_t simulation_steps = 0;
    std::uint64_t collision_count = 0;
    float simulated_seconds = 0.0f;
    float total_kinetic_energy = 0.0f;
    Vector3 center_of_mass;
};

enum class Preset {
    Empty,
    CollisionLab,
    DropTower,
    Fountain
};

class Space3DEngine {
public:
    Space3DEngine();

    std::uint32_t AddBall(const Vector3& position, const Vector3& velocity, float radius, float mass);
    bool RemoveBall(std::uint32_t id);
    Ball* FindBall(std::uint32_t id);
    const Ball* FindBall(std::uint32_t id) const;
    bool ApplyImpulse(std::uint32_t id, const Vector3& impulse);

    void Update(float elapsed_seconds);
    void StepOnce();
    void Clear();
    void Reset(Preset preset);
    void SetPaused(bool paused);
    bool IsPaused() const;

    RayHit Raycast(const Ray& ray) const;
    Diagnostics GetDiagnostics() const;
    std::wstring GetBallStats(std::uint32_t id) const;
    std::string Serialize() const;
    bool Deserialize(const std::string& text, std::string& error);

    const std::vector<Ball>& GetBalls() const;
    Camera& GetCamera();
    const Camera& GetCamera() const;
    SimulationSettings& GetSettings();
    const SimulationSettings& GetSettings() const;

private:
    void SimulateStep(float dt);
    void ResolveWorldCollision(Ball& ball);
    void ResolveBallCollisions();

    std::vector<Ball> balls_;
    Camera camera_;
    SimulationSettings settings_;
    std::uint32_t next_id_ = 1;
    bool paused_ = false;
    float accumulator_ = 0.0f;
    std::uint64_t simulation_steps_ = 0;
    std::uint64_t collision_count_ = 0;
    float simulated_seconds_ = 0.0f;
};

} // namespace space3d
