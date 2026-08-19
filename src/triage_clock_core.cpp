#include "triage_clock_core.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace triage_clock {

std::wstring Trim(std::wstring value) {
    const auto non_space = [](wchar_t ch) { return !std::iswspace(ch); };
    const auto first = std::find_if(value.begin(), value.end(), non_space);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if(value.rbegin(), value.rend(), non_space).base();
    return std::wstring(first, last);
}

std::wstring FormatDuration(int seconds) {
    seconds = std::max(0, seconds);
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    wchar_t buffer[32]{};
    if (hours > 0) {
        swprintf_s(buffer, L"%d:%02d:%02d", hours, minutes, secs);
    } else {
        swprintf_s(buffer, L"%02d:%02d", minutes, secs);
    }
    return buffer;
}

bool Plan::Configure(const std::vector<Action>& actions, std::wstring& error) {
    if (running_) {
        error = L"Reset the active plan before editing it.";
        return false;
    }
    if (actions.empty() || actions.size() > 3) {
        error = L"Enter between one and three next actions.";
        return false;
    }

    std::vector<Action> cleaned;
    cleaned.reserve(actions.size());
    for (const auto& action : actions) {
        Action item{Trim(action.title), action.minutes};
        if (item.title.empty()) {
            error = L"Each included action needs a title.";
            return false;
        }
        if (item.minutes < 1 || item.minutes > 480) {
            error = L"Minutes must be from 1 through 480.";
            return false;
        }
        cleaned.push_back(std::move(item));
    }

    actions_ = std::move(cleaned);
    active_index_ = 0;
    remaining_current_seconds_ = 0;
    finished_ = false;
    error.clear();
    return true;
}

bool Plan::Start(std::int64_t now_seconds, std::wstring& error) {
    if (actions_.empty()) {
        error = L"Add at least one action first.";
        return false;
    }
    if (running_) {
        error.clear();
        return true;
    }
    if (finished_) {
        active_index_ = 0;
        finished_ = false;
    }
    remaining_current_seconds_ = actions_[active_index_].minutes * 60;
    last_tick_seconds_ = now_seconds;
    running_ = true;
    error.clear();
    return true;
}

void Plan::MoveToNext() {
    ++active_index_;
    if (active_index_ >= actions_.size()) {
        remaining_current_seconds_ = 0;
        running_ = false;
        finished_ = true;
        return;
    }
    remaining_current_seconds_ = actions_[active_index_].minutes * 60;
}

void Plan::AdvanceTo(std::int64_t now_seconds) {
    if (!running_ || now_seconds <= last_tick_seconds_) {
        return;
    }

    std::int64_t elapsed = now_seconds - last_tick_seconds_;
    last_tick_seconds_ = now_seconds;
    while (running_ && elapsed >= remaining_current_seconds_) {
        elapsed -= remaining_current_seconds_;
        MoveToNext();
    }
    if (running_) {
        remaining_current_seconds_ -= static_cast<int>(elapsed);
    }
}

void Plan::Tick(std::int64_t now_seconds) {
    AdvanceTo(now_seconds);
}

void Plan::CompleteCurrent(std::int64_t now_seconds) {
    AdvanceTo(now_seconds);
    if (running_) {
        MoveToNext();
        last_tick_seconds_ = now_seconds;
    }
}

void Plan::Reset() {
    active_index_ = 0;
    remaining_current_seconds_ = 0;
    last_tick_seconds_ = 0;
    running_ = false;
    finished_ = false;
}

int Plan::total_remaining_seconds() const noexcept {
    if (actions_.empty() || finished_) {
        return 0;
    }

    int total = running_ ? remaining_current_seconds_ : actions_[active_index_].minutes * 60;
    for (std::size_t i = active_index_ + 1; i < actions_.size(); ++i) {
        total += actions_[i].minutes * 60;
    }
    return total;
}

}  // namespace triage_clock
