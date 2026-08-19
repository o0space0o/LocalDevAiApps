#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace streamer {

enum class SourceKind { Screen, Webcam };

struct StreamConfig {
    SourceKind source = SourceKind::Screen;
    std::wstring webcam_name;
    std::wstring server_url;
    std::wstring stream_key;
    std::wstring ffmpeg_path = L"ffmpeg.exe";
    int frame_rate = 30;
    int video_bitrate_kbps = 4500;
};

struct DesktopFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

std::wstring QuoteArgument(const std::wstring& value);
std::wstring BuildDestination(const StreamConfig& config);
std::wstring BuildFfmpegArguments(const StreamConfig& config, bool redact_secret);
bool ValidateConfig(const StreamConfig& config, std::wstring& error);
std::vector<std::wstring> EnumerateVideoDevices();
bool CaptureDesktop(DesktopFrame& frame, std::wstring& error);
bool SaveFrameAsBmp(const DesktopFrame& frame, const std::wstring& path, std::wstring& error);

class StreamProcess {
public:
    StreamProcess();
    ~StreamProcess();
    StreamProcess(const StreamProcess&) = delete;
    StreamProcess& operator=(const StreamProcess&) = delete;

    bool Start(const StreamConfig& config, std::wstring& error);
    void Stop();
    bool IsRunning();
    unsigned long ExitCode() const;

private:
    void* process_;
    unsigned long exit_code_;
};

} // namespace streamer
