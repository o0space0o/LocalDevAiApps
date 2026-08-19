#include "space_3d_routine.h"

#include <algorithm>
#include <cmath>

namespace space3d {
namespace {
constexpr std::array<std::wstring_view, 4> TaskNames{
    L"Standing / observing", L"Walking", L"Running", L"Jumping"
};
}

HumanoidRoutine::HumanoidRoutine(std::uint32_t seed) : random_(seed) {
    task_bag_ = {RoutineTask::Idle, RoutineTask::Walk, RoutineTask::Run, RoutineTask::Jump};
}

void HumanoidRoutine::Start() {
    if (!running_) {
        running_ = true;
        paused_ = false;
        BeginNextTask();
    } else {
        paused_ = false;
    }
}

void HumanoidRoutine::Pause(bool paused) noexcept {
    paused_ = paused;
}

void HumanoidRoutine::Reset(std::uint32_t seed) {
    random_.seed(seed);
    timer_.Reset();
    task_bag_ = {RoutineTask::Idle, RoutineTask::Walk, RoutineTask::Run, RoutineTask::Jump};
    bag_index_ = task_bag_.size();
    current_task_ = RoutineTask::Idle;
    input_ = {};
    clock_seconds_ = 0.0;
    completed_tasks_ = 0;
    completed_loops_ = 0;
    encountered_mask_ = 0;
    running_ = false;
    paused_ = false;
    jump_pending_ = false;
}

std::wstring_view HumanoidRoutine::current_task_name() const noexcept {
    return TaskNames[static_cast<std::size_t>(current_task_)];
}

double HumanoidRoutine::RandomDuration(RoutineTask task) {
    switch (task) {
    case RoutineTask::Idle: return std::uniform_real_distribution<double>(0.8, 1.5)(random_);
    case RoutineTask::Walk: return std::uniform_real_distribution<double>(1.4, 2.5)(random_);
    case RoutineTask::Run:  return std::uniform_real_distribution<double>(1.0, 1.8)(random_);
    case RoutineTask::Jump: return 1.45;
    default: return 1.0;
    }
}

void HumanoidRoutine::BeginNextTask() {
    if (bag_index_ >= task_bag_.size()) {
        std::shuffle(task_bag_.begin(), task_bag_.end(), random_);
        bag_index_ = 0;
    }

    current_task_ = task_bag_[bag_index_++];
    encountered_mask_ |= 1u << static_cast<unsigned>(current_task_);
    input_ = {};
    jump_pending_ = false;

    if (current_task_ == RoutineTask::Walk || current_task_ == RoutineTask::Run) {
        const int direction = std::uniform_int_distribution<int>(0, 7)(random_);
        const float angle = static_cast<float>(direction) * Pi / 4.0f;
        input_.forward = std::cos(angle);
        input_.right = std::sin(angle);
        input_.run = current_task_ == RoutineTask::Run;
    } else if (current_task_ == RoutineTask::Jump) {
        const int direction = std::uniform_int_distribution<int>(0, 7)(random_);
        const float angle = static_cast<float>(direction) * Pi / 4.0f;
        input_.forward = 0.35f * std::cos(angle);
        input_.right = 0.35f * std::sin(angle);
        jump_pending_ = true;
    }

    timer_.Start(clock_seconds_, RandomDuration(current_task_));
}

void HumanoidRoutine::Update(float elapsed_seconds, HumanoidController& body,
                             float ground_y, float half_extent) {
    if (elapsed_seconds <= 0.0f) return;

    if (!running_) {
        body.Update(elapsed_seconds, {}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, ground_y, half_extent);
        return;
    }
    if (paused_) {
        body.Update(elapsed_seconds, {}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, ground_y, half_extent);
        return;
    }

    clock_seconds_ += elapsed_seconds;
    timer_.Tick(clock_seconds_);
    if (timer_.expired()) {
        ++completed_tasks_;
        if (completed_tasks_ % task_bag_.size() == 0) ++completed_loops_;
        BeginNextTask();
    }

    HumanoidInput frame_input = input_;
    frame_input.jump = jump_pending_;
    jump_pending_ = false;
    body.Update(elapsed_seconds, frame_input,
                {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, ground_y, half_extent);
}

} // namespace space3d
