#include "space_3d_runtime.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace space3d {

BodyRuntime::BodyRuntime() = default;

void BodyRuntime::Start() {
    routine_.Start();
}

void BodyRuntime::Reset(HumanoidController& body, float ground_y) {
    body.Reset({0.0f, ground_y, 0.0f});
    routine_.Reset();
    automation_enabled_ = true;
    routine_.Start();
}

void BodyRuntime::ToggleAutomation() {
    automation_enabled_ = !automation_enabled_;
    routine_.Pause(!automation_enabled_);
    if (automation_enabled_) routine_.Start();
}

void BodyRuntime::Update(float elapsed_seconds, HumanoidController& body,
                         const HumanoidInput& manual_input, bool manual_active,
                         const Vector3& camera_forward, const Vector3& camera_right,
                         float ground_y, float half_extent) {
    if (manual_active) {
        routine_.Pause(true);
        body.Update(elapsed_seconds, manual_input, camera_forward, camera_right,
                    ground_y, half_extent);
        return;
    }

    routine_.Pause(!automation_enabled_);
    if (automation_enabled_) {
        routine_.Start();
        routine_.Update(elapsed_seconds, body, ground_y, half_extent);
    } else {
        body.Update(elapsed_seconds, {}, camera_forward, camera_right,
                    ground_y, half_extent);
    }
}

std::wstring BodyRuntime::LocalClockText() const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &value);
    std::wostringstream text;
    text << std::put_time(&local, L"%H:%M:%S");
    return text.str();
}

std::wstring BodyRuntime::StatusText() const {
    std::wostringstream text;
    text << (automation_enabled_ ? L"AUTOMATIC" : L"MANUAL")
         << L"\r\nTask: " << routine_.current_task_name()
         << L"\r\nTask time: " << std::fixed << std::setprecision(1)
         << routine_.remaining_seconds() << L" s"
         << L"\r\nTasks completed: " << routine_.completed_tasks()
         << L"\r\nRoutine loops: " << routine_.completed_loops();
    return text.str();
}

} // namespace space3d
