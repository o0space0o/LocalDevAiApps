#include "triage_clock_core.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kClassName[] = L"TriageClockWindow";
constexpr int kTimerId = 1;
constexpr int kStartId = 200;
constexpr int kCompleteId = 201;
constexpr int kResetId = 202;
constexpr int kStatusId = 203;
constexpr std::array<int, 3> kTitleIds{100, 102, 104};
constexpr std::array<int, 3> kMinuteIds{101, 103, 105};

triage_clock::Plan g_plan;
std::array<HWND, 3> g_titles{};
std::array<HWND, 3> g_minutes{};
HWND g_status = nullptr;
HWND g_start = nullptr;
HWND g_complete = nullptr;
HFONT g_font = nullptr;

std::int64_t MonotonicSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::wstring GetText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(copied));
    return text;
}

std::wstring FinishTimeText(int seconds_from_now) {
    const auto finish = std::chrono::system_clock::now() + std::chrono::seconds(seconds_from_now);
    const std::time_t value = std::chrono::system_clock::to_time_t(finish);
    std::tm local{};
    localtime_s(&local, &value);
    wchar_t buffer[64]{};
    wcsftime(buffer, std::size(buffer), L"%I:%M %p", &local);
    return buffer;
}

void SetFont(HWND control) {
    if (control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    }
}

HWND MakeControl(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style,
                 int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height,
                                   parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    SetFont(control);
    return control;
}

void EnableEditors(bool enabled) {
    for (HWND control : g_titles) EnableWindow(control, enabled);
    for (HWND control : g_minutes) EnableWindow(control, enabled);
}

void UpdateStatus() {
    std::wstring text;
    if (g_plan.running()) {
        const auto& action = g_plan.actions()[g_plan.active_index()];
        text = L"Now: " + action.title + L"\r\nRemaining: " +
               triage_clock::FormatDuration(g_plan.remaining_current_seconds()) +
               L"\r\nQueue finish: " + FinishTimeText(g_plan.total_remaining_seconds());
    } else if (g_plan.finished()) {
        text = L"Queue complete. Your next actions are done.";
    } else {
        text = L"Enter up to three actions in order. Blank rows are ignored.";
    }
    SetWindowTextW(g_status, text.c_str());
    EnableWindow(g_start, !g_plan.running());
    EnableWindow(g_complete, g_plan.running());
    EnableEditors(!g_plan.running());
}

bool ReadPlan(std::wstring& error) {
    std::vector<triage_clock::Action> actions;
    for (std::size_t i = 0; i < g_titles.size(); ++i) {
        std::wstring title = triage_clock::Trim(GetText(g_titles[i]));
        std::wstring minute_text = triage_clock::Trim(GetText(g_minutes[i]));
        if (title.empty() && minute_text.empty()) continue;
        if (title.empty() || minute_text.empty()) {
            error = L"A used row needs both an action and minutes.";
            return false;
        }
        wchar_t* end = nullptr;
        const long minutes = wcstol(minute_text.c_str(), &end, 10);
        if (end == minute_text.c_str() || *end != L'\0' || minutes < 1 || minutes > 480) {
            error = L"Minutes must be a whole number from 1 through 480.";
            return false;
        }
        actions.push_back({std::move(title), static_cast<int>(minutes)});
    }
    return g_plan.Configure(actions, error);
}

void ShowError(HWND owner, const std::wstring& message) {
    MessageBoxW(owner, message.c_str(), L"Triage Clock", MB_OK | MB_ICONWARNING);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        MakeControl(hwnd, L"STATIC", L"Triage Clock", SS_LEFT, 20, 16, 440, 28, 0);
        MakeControl(hwnd, L"STATIC", L"Action", SS_LEFT, 20, 52, 310, 20, 0);
        MakeControl(hwnd, L"STATIC", L"Minutes", SS_LEFT, 344, 52, 80, 20, 0);
        const wchar_t* defaults[3] = {L"Finish the current small task", L"Review the result", L"Choose what comes next"};
        const wchar_t* minute_defaults[3] = {L"25", L"5", L"2"};
        for (int i = 0; i < 3; ++i) {
            const int y = 76 + i * 38;
            g_titles[i] = MakeControl(hwnd, L"EDIT", defaults[i], WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                      20, y, 310, 26, kTitleIds[i]);
            g_minutes[i] = MakeControl(hwnd, L"EDIT", minute_defaults[i], WS_TABSTOP | WS_BORDER | ES_NUMBER,
                                       344, y, 80, 26, kMinuteIds[i]);
        }
        g_start = MakeControl(hwnd, L"BUTTON", L"Start queue", WS_TABSTOP | BS_DEFPUSHBUTTON,
                              20, 200, 120, 30, kStartId);
        g_complete = MakeControl(hwnd, L"BUTTON", L"Complete now", WS_TABSTOP,
                                 150, 200, 120, 30, kCompleteId);
        MakeControl(hwnd, L"BUTTON", L"Reset", WS_TABSTOP, 280, 200, 90, 30, kResetId);
        g_status = MakeControl(hwnd, L"STATIC", L"", SS_LEFT, 20, 248, 430, 70, kStatusId);
        UpdateStatus();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kStartId: {
            std::wstring error;
            if (!ReadPlan(error) || !g_plan.Start(MonotonicSeconds(), error)) {
                ShowError(hwnd, error);
                return 0;
            }
            SetTimer(hwnd, kTimerId, 250, nullptr);
            UpdateStatus();
            return 0;
        }
        case kCompleteId:
            g_plan.CompleteCurrent(MonotonicSeconds());
            if (!g_plan.running()) KillTimer(hwnd, kTimerId);
            UpdateStatus();
            return 0;
        case kResetId:
            KillTimer(hwnd, kTimerId);
            g_plan.Reset();
            UpdateStatus();
            return 0;
        default:
            break;
        }
        break;
    case WM_TIMER:
        if (wparam == kTimerId) {
            g_plan.Tick(MonotonicSeconds());
            if (!g_plan.running()) KillTimer(hwnd, kTimerId);
            UpdateStatus();
            return 0;
        }
        break;
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) return 1;

    HWND window = CreateWindowExW(0, kClassName, L"Triage Clock — Three-Action Focus Queue",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 480, 375, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;

    ShowWindow(window, show_command);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}
