#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace triage_clock {

struct Action {
    std::wstring title;
    int minutes = 0;
};

// Reusable monotonic countdown for applications that need sub-minute timed work.
class IntervalTimer {
public:
    bool Start(double now_seconds, double duration_seconds) noexcept;
    void Tick(double now_seconds) noexcept;
    void Reset() noexcept;

    bool running() const noexcept { return running_; }
    bool expired() const noexcept { return expired_; }
    double duration_seconds() const noexcept { return duration_seconds_; }
    double remaining_seconds() const noexcept { return remaining_seconds_; }

private:
    double deadline_seconds_ = 0.0;
    double duration_seconds_ = 0.0;
    double remaining_seconds_ = 0.0;
    bool running_ = false;
    bool expired_ = false;
};

class Plan {
public:
    bool Configure(const std::vector<Action>& actions, std::wstring& error);
    bool Start(std::int64_t now_seconds, std::wstring& error);
    void Tick(std::int64_t now_seconds);
    void CompleteCurrent(std::int64_t now_seconds);
    void Reset();

    bool running() const noexcept { return running_; }
    bool finished() const noexcept { return finished_; }
    std::size_t active_index() const noexcept { return active_index_; }
    int remaining_current_seconds() const noexcept { return remaining_current_seconds_; }
    int total_remaining_seconds() const noexcept;
    const std::vector<Action>& actions() const noexcept { return actions_; }

private:
    void AdvanceTo(std::int64_t now_seconds);
    void MoveToNext();

    std::vector<Action> actions_;
    std::size_t active_index_ = 0;
    int remaining_current_seconds_ = 0;
    std::int64_t last_tick_seconds_ = 0;
    bool running_ = false;
    bool finished_ = false;
};

std::wstring Trim(std::wstring value);
std::wstring FormatDuration(int seconds);
std::wstring FormatClockTime(int hour, int minute, int second);

} // namespace triage_clock
