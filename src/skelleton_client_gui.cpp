#include <windows.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include "skeleton_network.h"

namespace {

enum { ID_MODE_SERVER = 100, ID_MODE_CLIENT, ID_BACKEND, ID_START, ID_TIMER = 1 };

HWND mode_server = nullptr;
HWND mode_client = nullptr;
HWND backend_edit = nullptr;
HWND start_button = nullptr;
HWND status_box = nullptr;

std::unique_ptr<skeletonnet::SkeletonServer> server;
std::unique_ptr<skeletonnet::SkeletonClient> client;
space3d::HumanoidController body;
std::uint64_t sequence = 0;
float phase = 0;
std::wstring last_status = L"Stopped";

std::string Narrow(const std::wstring& value) {
    if (value.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring Widen(const std::string& value) {
    if (value.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), n);
    return out;
}

std::wstring EditText(HWND edit) {
    int n = GetWindowTextLengthW(edit);
    std::wstring value(n, L'\0');
    GetWindowTextW(edit, value.data(), n + 1);
    return value;
}

bool ServerMode() {
    return SendMessageW(mode_server, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void StopNetwork() {
    if (client) { client->Disconnect(); client.reset(); }
    if (server) { server->Stop(); server.reset(); }
    SetWindowTextW(start_button, L"Start");
    last_status = L"Stopped";
}

void StartNetwork() {
    StopNetwork();
    std::string error;
    if (ServerMode()) {
        skeletonnet::Endpoint endpoint;
        std::string text = Narrow(EditText(backend_edit));
        if (text.empty()) text = "0.0.0.0:3000";
        if (!skeletonnet::ParseEndpoint(text, endpoint, error)) {
            last_status = L"Invalid server URL: " + Widen(error);
            return;
        }
        server = std::make_unique<skeletonnet::SkeletonServer>();
        if (!server->Start(endpoint.port, error)) {
            server.reset();
            last_status = L"Server error: " + Widen(error);
            return;
        }
        last_status = L"Serving live skeletons on port " + std::to_wstring(server->Port());
    } else {
        client = std::make_unique<skeletonnet::SkeletonClient>();
        if (!client->Connect(Narrow(EditText(backend_edit)), error)) {
            client.reset();
            last_status = L"Client error: " + Widen(error);
            return;
        }
        last_status = L"Connected to " + EditText(backend_edit);
    }
    SetWindowTextW(start_button, L"Stop");
}

void PublishLocal() {
    space3d::HumanoidInput input;
    input.forward = 1.0f;
    input.right = std::sin(phase) * 0.3f;
    body.Update(0.033f, input, {0, 0, 1}, {1, 0, 0}, -5, 15);
    auto frame = skeletonnet::MakeFrame(
        ServerMode() ? "server-local" : "client-local",
        ServerMode() ? "Server skeleton" : "Client skeleton",
        ++sequence,
        body.BuildPose()
    );
    std::string error;
    if (server) server->Publish(frame, error);
    if (client) client->Publish(frame, error);
    if (!error.empty()) last_status = Widen(error);
    phase += 0.033f;
}

std::vector<skeletonnet::SkeletonFrame> Frames() {
    if (server) return server->Frames();
    if (client) return client->Frames();
    return {};
}

void RefreshStatus() {
    auto frames = Frames();
    std::wostringstream text;
    text << L"SKELLETON CLIENT — protocol v" << skeletonnet::ProtocolVersion << L"\r\n";
    text << last_status << L"\r\n";
    if (server) text << L"Connected clients: " << server->ClientCount() << L"\r\n";
    text << L"Visible live skeletons: " << frames.size() << L"\r\n\r\n";
    for (const auto& f : frames) {
        const auto& p = f.joints[0];
        text << Widen(f.display_name) << L" [" << Widen(f.source_id) << L"] seq " << f.sequence
             << L" pelvis (" << static_cast<int>(p.x * 100) / 100.0 << L", "
             << static_cast<int>(p.y * 100) / 100.0 << L", "
             << static_cast<int>(p.z * 100) / 100.0 << L")\r\n";
    }
    SetWindowTextW(status_box, text.str().c_str());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"Mode:", WS_CHILD | WS_VISIBLE, 16, 18, 50, 22, hwnd, nullptr, nullptr, nullptr);
        mode_server = CreateWindowW(L"BUTTON", L"Server",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 70, 14, 80, 26,
            hwnd, reinterpret_cast<HMENU>(ID_MODE_SERVER), nullptr, nullptr);
        mode_client = CreateWindowW(L"BUTTON", L"Client",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 154, 14, 80, 26,
            hwnd, reinterpret_cast<HMENU>(ID_MODE_CLIENT), nullptr, nullptr);
        SendMessageW(mode_client, BM_SETCHECK, BST_CHECKED, 0);
        CreateWindowW(L"STATIC", L"Backend URL / listen port:",
            WS_CHILD | WS_VISIBLE, 16, 54, 190, 22, hwnd, nullptr, nullptr, nullptr);
        backend_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"tcp://127.0.0.1:3000",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, 78, 430, 28,
            hwnd, reinterpret_cast<HMENU>(ID_BACKEND), nullptr, nullptr);
        start_button = CreateWindowW(L"BUTTON", L"Start",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 458, 77, 90, 30,
            hwnd, reinterpret_cast<HMENU>(ID_START), nullptr, nullptr);
        status_box = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Stopped",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 16, 122, 532, 300,
            hwnd, nullptr, nullptr, nullptr);
        body.Reset({0, -5, 0});
        SetTimer(hwnd, ID_TIMER, 33, nullptr);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_START) {
            if (server || client) StopNetwork();
            else StartNetwork();
            RefreshStatus();
        }
        return 0;
    case WM_TIMER:
        if (server || client) PublishLocal();
        RefreshStatus();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        StopNetwork();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // anonymous namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"LocalDevAiSkelletonClient";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&wc)) return 1;
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"SkelletonClient — Live 3D Body Server / Client",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 480, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 2;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}
