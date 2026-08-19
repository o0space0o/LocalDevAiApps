#include "triage_clock_core.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
    std::cerr << "TRIAGE_CLOCK_PROBE_FAIL: " << message << '\n';
    return 1;
}

}  // namespace

int wmain() {
    triage_clock::Plan plan;
    std::wstring error;

    if (plan.Configure({{L" Draft update ", 2}, {L"Review update", 1}}, error) == false) {
        return Fail("valid queue rejected");
    }
    if (plan.actions().size() != 2 || plan.actions()[0].title != L"Draft update") {
        return Fail("queue normalization failed");
    }
    if (!plan.Start(1000, error)) {
        return Fail("queue did not start");
    }
    plan.Tick(1030);
    if (!plan.running() || plan.active_index() != 0 || plan.remaining_current_seconds() != 90 ||
        plan.total_remaining_seconds() != 150) {
        return Fail("countdown or finish forecast is incorrect");
    }

    plan.CompleteCurrent(1030);
    if (!plan.running() || plan.active_index() != 1 || plan.remaining_current_seconds() != 60 ||
        plan.total_remaining_seconds() != 60) {
        return Fail("complete-now did not advance the queue");
    }

    plan.Tick(1090);
    if (plan.running() || !plan.finished() || plan.total_remaining_seconds() != 0) {
        return Fail("queue did not finish at the expected time");
    }

    plan.Reset();
    if (plan.running() || plan.finished() || plan.active_index() != 0) {
        return Fail("reset failed");
    }

    if (plan.Configure({{L"Invalid", 0}}, error) || error.empty()) {
        return Fail("invalid duration was accepted");
    }

    std::cout << "TRIAGE_CLOCK_PROBE_OK actions=2 countdown=90 forecast=150 advance=ok reset=ok\n";
    return 0;
}
