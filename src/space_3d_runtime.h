#pragma once

#include <cstdint>
#include <string>
#include "space_3d_routine.h"
#include "triage_clock_core.h"

namespace space3d {

// Coordinates the independently reusable timed routine and clock services for space_3d.
class BodyRuntime {
public:
    BodyRuntime();
    void Start();
    void Reset(HumanoidController& body, float ground_y);
    void ToggleAutomation();
    void Update(float elapsed_seconds, HumanoidController& body,
                const HumanoidInput& manual_input, bool manual_active,
                const Vector3& camera_forward, const Vector3& camera_right,
                float ground_y, float half_extent);

    bool automation_enabled() const noexcept { return automation_enabled_; }
    const HumanoidRoutine& routine() const noexcept { return routine_; }
    std::wstring LocalClockText() const;
    std::wstring StatusText() const;

private:
    HumanoidRoutine routine_;
    bool automation_enabled_ = true;
};

} // namespace space3d
