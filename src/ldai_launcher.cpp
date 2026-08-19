#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

int wmain(int argc, wchar_t** argv) {
    namespace fs = std::filesystem;
    fs::path exePath = argc > 0 ? fs::path(argv[0]) : fs::current_path();
    fs::path dir = exePath.parent_path();
    if (dir.empty()) dir = fs::current_path();

    std::vector<std::wstring> candidates = {L"space_3d.exe", L"triage_clock.exe", L"video_streamer.exe"};
    std::vector<std::pair<std::wstring, bool>> results;
    for (const auto &c : candidates) {
        bool exists = fs::exists(dir / c);
        results.push_back({c, exists});
    }

    std::wcout << L"Launcher inspection for folder: " << dir.wstring() << L"\n";
    int found = 0;
    for (const auto &r : results) {
        std::wcout << (r.second ? L"[FOUND] " : L"[MISSING] ") << r.first << L"\n";
        if (r.second) ++found;
    }

    // Behavior probe: require at least space_3d and triage_clock to be present
    bool ok = fs::exists(dir / L"space_3d.exe") && fs::exists(dir / L"triage_clock.exe");
    if (ok) {
        std::wcout << L"LDAI_LAUNCHER_OK\n";
        return 0;
    }
    std::wcerr << L"LDAI_LAUNCHER_FAIL: required apps missing\n";
    return 2;
}
