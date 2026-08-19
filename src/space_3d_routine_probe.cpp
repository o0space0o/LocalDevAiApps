#include "space_3d_routine.h"
#include "space_3d_runtime.h"

#include <cmath>
#include <cwctype>
#include <iostream>

namespace {
bool IsClockText(const std::wstring& value) {
    if (value.size() != 8 || value[2] != L':' || value[5] != L':') return false;
    for (const int index : {0, 1, 3, 4, 6, 7}) {
        if (!std::iswdigit(value[static_cast<std::size_t>(index)])) return false;
    }
    const int hour = (value[0] - L'0') * 10 + value[1] - L'0';
    const int minute = (value[3] - L'0') * 10 + value[4] - L'0';
    const int second = (value[6] - L'0') * 10 + value[7] - L'0';
    return hour < 24 && minute < 60 && second < 60;
}
}

int main() {
    space3d::HumanoidController body;
    space3d::HumanoidRoutine routine(123456u);
    routine.Start();

    const auto start = body.GetState().position;
    bool saw_airborne = false;
    bool landed_after_jump = false;
    bool saw_walk_speed = false;
    bool saw_run_speed = false;
    float farthest = 0.0f;

    for (int i = 0; i < 4000 && routine.completed_loops() < 2; ++i) {
        routine.Update(0.01f, body, -5.0f, 15.0f);
        const auto& state = body.GetState();
        const float dx = state.position.x - start.x;
        const float dz = state.position.z - start.z;
        farthest = std::max(farthest, std::sqrt(dx * dx + dz * dz));
        if (!state.grounded) saw_airborne = true;
        if (saw_airborne && state.grounded) landed_after_jump = true;
        if (routine.current_task() == space3d::RoutineTask::Walk &&
            std::abs(state.horizontal_speed_mps - space3d::HumanoidController::WalkSpeedMetersPerSecond) < 0.08f) {
            saw_walk_speed = true;
        }
        if (routine.current_task() == space3d::RoutineTask::Run &&
            std::abs(state.horizontal_speed_mps - space3d::HumanoidController::RunSpeedMetersPerSecond) < 0.08f) {
            saw_run_speed = true;
        }
    }

    const double before_pause = routine.remaining_seconds();
    routine.Pause(true);
    for (int i = 0; i < 100; ++i) routine.Update(0.01f, body, -5.0f, 15.0f);
    const bool pause_holds_timer = std::abs(routine.remaining_seconds() - before_pause) < 0.0001;

    space3d::HumanoidController integrated_body;
    space3d::BodyRuntime runtime;
    runtime.Start();
    for (int i = 0; i < 120; ++i) {
        runtime.Update(0.016f, integrated_body, {}, false,
                       {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, -5.0f, 15.0f);
    }
    const std::wstring live_clock = runtime.LocalClockText();
    const std::wstring runtime_status = runtime.StatusText();
    const bool runtime_integrated = IsClockText(live_clock) &&
        runtime_status.find(L"AUTOMATIC") != std::wstring::npos &&
        runtime_status.find(L"Task:") != std::wstring::npos &&
        runtime_status.find(L"Task time:") != std::wstring::npos;

    const bool all_tasks = routine.encountered_mask() == 0x0Fu;
    const bool looped = routine.completed_loops() >= 2 && routine.completed_tasks() >= 8;
    const bool clock_format = triage_clock::FormatClockTime(23, 7, 5) == L"23:07:05";

    if (!all_tasks || !looped || !saw_airborne || !landed_after_jump ||
        !saw_walk_speed || !saw_run_speed || farthest < 1.0f ||
        !pause_holds_timer || !clock_format || !runtime_integrated) {
        std::cerr << "routine probe failed mask=" << routine.encountered_mask()
                  << " loops=" << routine.completed_loops()
                  << " airborne=" << saw_airborne << " landed=" << landed_after_jump
                  << " walk=" << saw_walk_speed << " run=" << saw_run_speed
                  << " distance=" << farthest << " pause=" << pause_holds_timer
                  << " runtime=" << runtime_integrated << "\n";
        return 1;
    }

    std::cout << "SPACE_3D_ROUTINE_PROBE_OK tasks=randomized-timed-loop"
              << " loops=" << routine.completed_loops()
              << " walk=1.4mps run=3.5mps jump=landed rtc=live-24h pause=ok integration=ok\n";
    return 0;
}
