#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string_view>

#include "skeleton_body.h"
#include "triage_clock_core.h"

namespace space3d {

enum class RoutineTask : std::uint8_t { Idle, Walk, Run, Jump, Count };

class HumanoidRoutine {
public:
    explicit HumanoidRoutine(std::uint32_t seed = 0x534B454Cu);

    void Start();
    void Pause(bool paused) noexcept;
    void Reset(std::uint32_t seed = 0x534B454Cu);
    void Update(float elapsed_seconds, HumanoidController& body,
                float ground_y, float half_extent);

    bool running() const noexcept { return running_; }
    bool paused() const noexcept { return paused_; }
    RoutineTask current_task() const noexcept { return current_task_; }
    std::wstring_view current_task_name() const noexcept;
    double remaining_seconds() const noexcept { return timer_.remaining_seconds(); }
    std::uint64_t completed_tasks() const noexcept { return completed_tasks_; }
    std::uint64_t completed_loops() const noexcept { return completed_loops_; }
    std::uint32_t encountered_mask() const noexcept { return encountered_mask_; }

private:
    void BeginNextTask();
    double RandomDuration(RoutineTask task);

    triage_clock::IntervalTimer timer_;
    std::mt19937 random_;
    std::array<RoutineTask, 4> task_bag_{};
    std::size_t bag_index_ = 4;
    RoutineTask current_task_ = RoutineTask::Idle;
    HumanoidInput input_{};
    double clock_seconds_ = 0.0;
    std::uint64_t completed_tasks_ = 0;
    std::uint64_t completed_loops_ = 0;
    std::uint32_t encountered_mask_ = 0;
    bool running_ = false;
    bool paused_ = false;
    bool jump_pending_ = false;
};

} // namespace space3d
