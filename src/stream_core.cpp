#define WIN32_LEAN_AND_MEAN
#include "stream_core.h"

#include <windows.h>
#include <dshow.h>
#include <algorithm>
#include <cwctype>
#include <sstream>

namespace streamer {
namespace {

std::wstring TrimSlashes(std::wstring value) {
    while (!value.empty() && (value.back() == L'/' || value.back() == L'\\')) value.pop_back();
    return value;
}

bool StartsWithInsensitive(const std::wstring& value, const std::wstring& prefix) {
    if (value.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::towlower(value[i]) != std::towlower(prefix[i])) return false;
    }
    return true;
}

} // namespace

std::wstring QuoteArgument(const std::wstring& value) {
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++slashes;
        } else if (ch == L'\"') {
            out.append(slashes * 2 + 1, L'\\');
            out.push_back(L'\"');
            slashes = 0;
        } else {
            out.append(slashes, L'\\');
            slashes = 0;
            out.push_back(ch);
        }
    }
    out.append(slashes * 2, L'\\');
    out.push_back(L'\"');
    return out;
}

std::wstring BuildDestination(const StreamConfig& config) {
    std::wstring base = TrimSlashes(config.server_url);
    if (config.stream_key.empty()) return base;
    return base + L"/" + config.stream_key;
}

std::wstring BuildFfmpegArguments(const StreamConfig& config, bool redact_secret) {
    std::wostringstream command;
    command << L"-hide_banner -loglevel warning -nostdin -y ";
    if (config.source == SourceKind::Screen) {
        command << L"-f gdigrab -framerate " << config.frame_rate << L" -i desktop ";
    } else {
        command << L"-f dshow -i " << QuoteArgument(L"video=" + config.webcam_name) << L" ";
    }
    command << L"-f lavfi -i " << QuoteArgument(L"anullsrc=channel_layout=stereo:sample_rate=44100") << L" ";
    command << L"-map 0:v:0 -map 1:a:0 -c:v libx264 -preset veryfast -tune zerolatency "
            << L"-pix_fmt yuv420p -r " << config.frame_rate << L" -g " << (config.frame_rate * 2)
            << L" -b:v " << config.video_bitrate_kbps << L"k -maxrate " << config.video_bitrate_kbps
            << L"k -bufsize " << (config.video_bitrate_kbps * 2)
            << L"k -c:a aac -b:a 128k -ar 44100 -shortest -f flv ";
    const std::wstring destination = redact_secret
        ? TrimSlashes(config.server_url) + L"/[STREAM_KEY_REDACTED]"
        : BuildDestination(config);
    command << QuoteArgument(destination);
    return command.str();
}

bool ValidateConfig(const StreamConfig& config, std::wstring& error) {
    if (config.ffmpeg_path.empty()) { error = L"Select ffmpeg.exe first."; return false; }
    if (config.server_url.empty()) { error = L"Paste the server URL from YouTube or Facebook Live."; return false; }
    if (!StartsWithInsensitive(config.server_url, L"rtmp://") && !StartsWithInsensitive(config.server_url, L"rtmps://")) {
        error = L"The server URL must start with rtmp:// or rtmps://."; return false;
    }
    if (config.stream_key.empty()) { error = L"Enter the stream key."; return false; }
    if (config.stream_key.find_first_of(L"\r\n") != std::wstring::npos) { error = L"The stream key contains an invalid newline."; return false; }
    if (config.source == SourceKind::Webcam && config.webcam_name.empty()) { error = L"Select a webcam."; return false; }
    if (config.frame_rate < 1 || config.frame_rate > 60) { error = L"Frame rate must be between 1 and 60."; return false; }
    return true;
}

std::vector<std::wstring> EnumerateVideoDevices() {
    std::vector<std::wstring> devices;
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool should_uninitialize = SUCCEEDED(init);

    ICreateDevEnum* enumerator = nullptr;
    IEnumMoniker* monikers = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ICreateDevEnum, reinterpret_cast<void**>(&enumerator)))) {
        if (enumerator->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &monikers, 0) == S_OK) {
            IMoniker* moniker = nullptr;
            while (monikers->Next(1, &moniker, nullptr) == S_OK) {
                IPropertyBag* bag = nullptr;
                if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                                     reinterpret_cast<void**>(&bag)))) {
                    VARIANT name;
                    VariantInit(&name);
                    if (SUCCEEDED(bag->Read(L"FriendlyName", &name, nullptr)) && name.vt == VT_BSTR) {
                        devices.emplace_back(name.bstrVal);
                    }
                    VariantClear(&name);
                    bag->Release();
                }
                moniker->Release();
            }
            monikers->Release();
        }
        enumerator->Release();
    }
    if (should_uninitialize) CoUninitialize();
    return devices;
}

bool CaptureDesktop(DesktopFrame& frame, std::wstring& error) {
    frame = {};
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0 || height <= 0) { error = L"Windows reported an empty virtual desktop."; return false; }

    HDC screen = GetDC(nullptr);
    HDC memory = screen ? CreateCompatibleDC(screen) : nullptr;
    HBITMAP bitmap = memory ? CreateCompatibleBitmap(screen, width, height) : nullptr;
    if (!screen || !memory || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        error = L"Could not allocate desktop capture resources.";
        return false;
    }

    HGDIOBJ old = SelectObject(memory, bitmap);
    const BOOL copied = BitBlt(memory, 0, 0, width, height, screen, left, top, SRCCOPY | CAPTUREBLT);
    frame.width = width;
    frame.height = height;
    frame.bgra.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const int lines = copied ? GetDIBits(memory, bitmap, 0, height, frame.bgra.data(), &info, DIB_RGB_COLORS) : 0;

    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    if (!copied || lines != height) { frame = {}; error = L"Desktop pixels could not be read."; return false; }
    return true;
}

bool SaveFrameAsBmp(const DesktopFrame& frame, const std::wstring& path, std::wstring& error) {
    if (frame.width <= 0 || frame.height <= 0 || frame.bgra.size() != static_cast<size_t>(frame.width) * frame.height * 4) {
        error = L"The desktop frame is invalid."; return false;
    }
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = L"Could not create the probe image."; return false; }
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER image_header{};
    image_header.biSize = sizeof(image_header);
    image_header.biWidth = frame.width;
    image_header.biHeight = -frame.height;
    image_header.biPlanes = 1;
    image_header.biBitCount = 32;
    image_header.biCompression = BI_RGB;
    image_header.biSizeImage = static_cast<DWORD>(frame.bgra.size());
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(image_header);
    file_header.bfSize = file_header.bfOffBits + image_header.biSizeImage;
    DWORD written = 0;
    bool ok = WriteFile(file, &file_header, sizeof(file_header), &written, nullptr) && written == sizeof(file_header)
           && WriteFile(file, &image_header, sizeof(image_header), &written, nullptr) && written == sizeof(image_header)
           && WriteFile(file, frame.bgra.data(), static_cast<DWORD>(frame.bgra.size()), &written, nullptr)
           && written == frame.bgra.size();
    CloseHandle(file);
    if (!ok) error = L"The probe image could not be written completely.";
    return ok;
}

StreamProcess::StreamProcess() : process_(nullptr), exit_code_(STILL_ACTIVE) {}
StreamProcess::~StreamProcess() { Stop(); }

bool StreamProcess::Start(const StreamConfig& config, std::wstring& error) {
    Stop();
    if (!ValidateConfig(config, error)) return false;
    std::wstring command = QuoteArgument(config.ffmpeg_path) + L" " + BuildFfmpegArguments(config, false);
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        std::wostringstream text;
        text << L"Could not start FFmpeg (Windows error " << GetLastError() << L").";
        error = text.str();
        return false;
    }
    CloseHandle(process.hThread);
    process_ = process.hProcess;
    exit_code_ = STILL_ACTIVE;
    return true;
}

void StreamProcess::Stop() {
    if (!process_) return;
    HANDLE process = static_cast<HANDLE>(process_);
    DWORD code = STILL_ACTIVE;
    if (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE) {
        TerminateProcess(process, 0);
        WaitForSingleObject(process, 3000);
        GetExitCodeProcess(process, &code);
    }
    exit_code_ = code;
    CloseHandle(process);
    process_ = nullptr;
}

bool StreamProcess::IsRunning() {
    if (!process_) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(static_cast<HANDLE>(process_), &code)) return false;
    exit_code_ = code;
    if (code == STILL_ACTIVE) return true;
    CloseHandle(static_cast<HANDLE>(process_));
    process_ = nullptr;
    return false;
}

unsigned long StreamProcess::ExitCode() const { return exit_code_; }

} // namespace streamer
