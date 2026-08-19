#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include "stream_core.h"

int Fail(const std::wstring& message) {
    std::wcerr << L"STREAM_PROBE_FAIL: " << message << L"\n";
    return 1;
}

int wmain() {
    streamer::DesktopFrame frame;
    std::wstring error;
    if (!streamer::CaptureDesktop(frame, error)) return Fail(error);
    if (frame.width < 320 || frame.height < 200 || frame.bgra.empty()) return Fail(L"Captured desktop dimensions are implausible.");

    std::uint8_t minimum = 255, maximum = 0;
    std::uint64_t luma_sum = 0;
    for (size_t i = 0; i + 3 < frame.bgra.size(); i += 4) {
        const std::uint8_t luma = static_cast<std::uint8_t>((frame.bgra[i] * 29 + frame.bgra[i+1] * 150 + frame.bgra[i+2] * 77) >> 8);
        minimum = std::min(minimum, luma); maximum = std::max(maximum, luma); luma_sum += luma;
    }
    const auto pixels = frame.bgra.size() / 4;
    if (maximum - minimum < 4 || luma_sum / pixels < 2) return Fail(L"Desktop capture contains no useful visual variation.");

    wchar_t executable[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return Fail(L"Cannot locate probe executable.");
    std::wstring image_path(executable);
    image_path = image_path.substr(0, image_path.find_last_of(L"\\/")) + L"\\stream_probe.bmp";
    if (!streamer::SaveFrameAsBmp(frame, image_path, error)) return Fail(error);

    streamer::StreamConfig screen;
    screen.source = streamer::SourceKind::Screen;
    screen.server_url = L"rtmps://example.invalid/live";
    screen.stream_key = L"SECRET_TEST_KEY";
    screen.ffmpeg_path = L"C:\\Program Files\\ffmpeg\\ffmpeg.exe";
    if (!streamer::ValidateConfig(screen, error)) return Fail(L"Valid screen configuration was rejected: " + error);
    const std::wstring actual = streamer::BuildFfmpegArguments(screen, false);
    const std::wstring redacted = streamer::BuildFfmpegArguments(screen, true);
    if (actual.find(L"gdigrab") == std::wstring::npos || actual.find(L"libx264") == std::wstring::npos || actual.find(L"-f flv") == std::wstring::npos)
        return Fail(L"Screen streaming command is missing capture, encoding, or FLV output.");
    if (actual.find(screen.stream_key) == std::wstring::npos) return Fail(L"Destination key was not attached to the server URL.");
    if (redacted.find(screen.stream_key) != std::wstring::npos || redacted.find(L"[STREAM_KEY_REDACTED]") == std::wstring::npos)
        return Fail(L"Diagnostic command did not redact the stream key.");

    streamer::StreamConfig webcam = screen;
    webcam.source = streamer::SourceKind::Webcam;
    webcam.webcam_name = L"Camera Name With Spaces";
    const std::wstring webcam_args = streamer::BuildFfmpegArguments(webcam, true);
    if (webcam_args.find(L"-f dshow") == std::wstring::npos || webcam_args.find(L"video=Camera Name With Spaces") == std::wstring::npos)
        return Fail(L"Webcam streaming command is missing its DirectShow source.");

    streamer::StreamConfig invalid = screen;
    invalid.server_url = L"https://not-an-rtmp-endpoint";
    if (streamer::ValidateConfig(invalid, error)) return Fail(L"Invalid HTTP destination was accepted.");

    std::wcout << L"Captured " << frame.width << L"x" << frame.height << L" desktop to " << image_path << L"\n";
    std::cout << "STREAM_PROBE_OK\n";
    return 0;
}
