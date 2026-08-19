#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include "stream_core.h"
#include "webcam_preview.h"

namespace {
enum : int { ID_SOURCE=100, ID_CAMERA, ID_SERVER, ID_KEY, ID_FFMPEG, ID_BROWSE, ID_START, ID_STATUS };
HWND g_source{}, g_camera{}, g_server{}, g_key{}, g_ffmpeg{}, g_start{}, g_status{};
streamer::DesktopFrame g_frame;
streamer::StreamProcess g_stream;
WebcamPreview g_webcam;
bool g_live = false;
const RECT kPreview{20, 250, 940, 710};

std::wstring Text(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::vector<wchar_t> data(static_cast<size_t>(length) + 1);
    GetWindowTextW(control, data.data(), length + 1);
    return data.data();
}
void Status(const std::wstring& value) { SetWindowTextW(g_status, value.c_str()); }
streamer::SourceKind Source() { return SendMessageW(g_source, CB_GETCURSEL, 0, 0) == 1 ? streamer::SourceKind::Webcam : streamer::SourceKind::Screen; }
std::wstring SelectedCamera() {
    int index = static_cast<int>(SendMessageW(g_camera, CB_GETCURSEL, 0, 0));
    if (index < 0) return L"";
    wchar_t value[512]{}; SendMessageW(g_camera, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(value)); return value;
}
streamer::StreamConfig Config() {
    streamer::StreamConfig c;
    c.source = Source(); c.webcam_name = SelectedCamera(); c.server_url = Text(g_server);
    c.stream_key = Text(g_key); c.ffmpeg_path = Text(g_ffmpeg); return c;
}
void SetControlsEnabled(bool enabled) {
    EnableWindow(g_source, enabled); EnableWindow(g_camera, enabled); EnableWindow(g_server, enabled);
    EnableWindow(g_key, enabled); EnableWindow(g_ffmpeg, enabled); EnableWindow(GetDlgItem(GetParent(g_start), ID_BROWSE), enabled);
}
void StartWebcamPreview(HWND hwnd) {
    g_webcam.Stop();
    if (g_live || Source() != streamer::SourceKind::Webcam) return;
    std::wstring camera = SelectedCamera();
    if (camera.empty()) { Status(L"No webcam found. Choose Screen or connect a camera."); return; }
    std::wstring error;
    if (!g_webcam.Start(hwnd, camera, error)) Status(L"Preview error: " + error);
    else { g_webcam.Resize(kPreview.left, kPreview.top, kPreview.right-kPreview.left, kPreview.bottom-kPreview.top); Status(L"Webcam preview ready (not live)."); }
}
void UpdateSource(HWND hwnd) {
    bool camera = Source() == streamer::SourceKind::Webcam;
    ShowWindow(g_camera, camera ? SW_SHOW : SW_HIDE);
    if (camera) StartWebcamPreview(hwnd); else { g_webcam.Stop(); Status(L"Screen preview ready (not live)."); }
    InvalidateRect(hwnd, &kPreview, TRUE);
}
void BrowseFfmpeg(HWND hwnd) {
    wchar_t file[MAX_PATH] = L"ffmpeg.exe";
    OPENFILENAMEW open{}; open.lStructSize=sizeof(open); open.hwndOwner=hwnd; open.lpstrFile=file; open.nMaxFile=MAX_PATH;
    open.lpstrFilter=L"FFmpeg executable\0ffmpeg.exe\0Programs\0*.exe\0"; open.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&open)) SetWindowTextW(g_ffmpeg, file);
}
void ToggleStream(HWND hwnd) {
    if (g_live) {
        g_stream.Stop(); g_live=false; SetWindowTextW(g_start,L"Start live stream"); SetControlsEnabled(true);
        Status(L"OFFLINE - stream stopped."); if (Source()==streamer::SourceKind::Webcam) StartWebcamPreview(hwnd); return;
    }
    streamer::StreamConfig config=Config(); std::wstring error;
    if (!streamer::ValidateConfig(config,error)) { MessageBoxW(hwnd,error.c_str(),L"Cannot start",MB_ICONWARNING); return; }
    g_webcam.Stop();
    if (!g_stream.Start(config,error)) { MessageBoxW(hwnd,error.c_str(),L"FFmpeg error",MB_ICONERROR); StartWebcamPreview(hwnd); return; }
    g_live=true; SetWindowTextW(g_start,L"Stop stream"); SetControlsEnabled(false);
    Status(L"CONNECTING - FFmpeg started. Confirm LIVE in your provider dashboard.");
}
void PaintPreview(HWND hwnd, HDC dc) {
    RECT r=kPreview; FillRect(dc,&r,reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    if (Source()==streamer::SourceKind::Screen && !g_frame.bgra.empty()) {
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=g_frame.width;
        info.bmiHeader.biHeight=-g_frame.height; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        SetStretchBltMode(dc,HALFTONE);
        StretchDIBits(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,0,0,g_frame.width,g_frame.height,g_frame.bgra.data(),&info,DIB_RGB_COLORS,SRCCOPY);
    } else if (Source()==streamer::SourceKind::Webcam && !g_webcam.Running()) {
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(220,220,220));
        std::wstring note=g_live?L"Webcam is owned by FFmpeg while streaming.\nWatch the live status in YouTube/Facebook Studio."
                                :L"Select an available webcam above.";
        DrawTextW(dc,note.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_WORDBREAK|DT_SINGLELINE);
    }
    FrameRect(dc,&r,reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
}
HWND Label(HWND parent,const wchar_t* text,int x,int y,int w=145) { return CreateWindowW(L"STATIC",text,WS_CHILD|WS_VISIBLE,x,y,w,22,parent,nullptr,nullptr,nullptr); }
HWND Edit(HWND parent,int id,int x,int y,int w,DWORD extra=0) { return CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|extra,x,y,w,25,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr); }

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        Label(hwnd,L"Capture source",20,18); g_source=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,170,15,260,300,hwnd,(HMENU)ID_SOURCE,nullptr,nullptr);
        SendMessageW(g_source,CB_ADDSTRING,0,(LPARAM)L"Entire desktop"); SendMessageW(g_source,CB_ADDSTRING,0,(LPARAM)L"Webcam"); SendMessageW(g_source,CB_SETCURSEL,0,0);
        g_camera=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|CBS_DROPDOWNLIST,450,15,490,300,hwnd,(HMENU)ID_CAMERA,nullptr,nullptr);
        for(const auto& name:streamer::EnumerateVideoDevices()) SendMessageW(g_camera,CB_ADDSTRING,0,(LPARAM)name.c_str());
        SendMessageW(g_camera,CB_SETCURSEL,0,0);
        Label(hwnd,L"Server URL",20,57); g_server=Edit(hwnd,ID_SERVER,170,54,770); SetWindowTextW(g_server,L"rtmps://");
        Label(hwnd,L"Stream key",20,96); g_key=Edit(hwnd,ID_KEY,170,93,770,ES_PASSWORD); SendMessageW(g_key,EM_SETPASSWORDCHAR,0x2022,0);
        Label(hwnd,L"FFmpeg",20,135); g_ffmpeg=Edit(hwnd,ID_FFMPEG,170,132,650); SetWindowTextW(g_ffmpeg,L"ffmpeg.exe");
        CreateWindowW(L"BUTTON",L"Browse...",WS_CHILD|WS_VISIBLE,830,131,110,27,hwnd,(HMENU)ID_BROWSE,nullptr,nullptr);
        g_start=CreateWindowW(L"BUTTON",L"Start live stream",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,20,177,190,38,hwnd,(HMENU)ID_START,nullptr,nullptr);
        g_status=CreateWindowW(L"STATIC",L"Screen preview ready (not live).",WS_CHILD|WS_VISIBLE,230,184,710,25,hwnd,(HMENU)ID_STATUS,nullptr,nullptr);
        SetTimer(hwnd,1,250,nullptr); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==ID_SOURCE && HIWORD(wp)==CBN_SELCHANGE) UpdateSource(hwnd);
        else if(LOWORD(wp)==ID_CAMERA && HIWORD(wp)==CBN_SELCHANGE) StartWebcamPreview(hwnd);
        else if(LOWORD(wp)==ID_BROWSE) BrowseFfmpeg(hwnd);
        else if(LOWORD(wp)==ID_START) ToggleStream(hwnd);
        return 0;
    case WM_TIMER: {
        if(Source()==streamer::SourceKind::Screen) { std::wstring ignored; streamer::CaptureDesktop(g_frame,ignored); InvalidateRect(hwnd,&kPreview,FALSE); }
        if(g_live && !g_stream.IsRunning()) { g_live=false; SetControlsEnabled(true); SetWindowTextW(g_start,L"Start live stream"); Status(L"ERROR - FFmpeg exited. Check the URL, key, camera, and FFmpeg codecs."); }
        else if(g_live) Status(L"LIVE PROCESS RUNNING - verify the broadcast preview in your provider dashboard.");
        return 0;
    }
    case WM_PAINT: { PAINTSTRUCT ps{}; HDC dc=BeginPaint(hwnd,&ps); PaintPreview(hwnd,dc); EndPaint(hwnd,&ps); return 0; }
    case WM_DESTROY: KillTimer(hwnd,1); g_webcam.Stop(); g_stream.Stop(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=instance; wc.lpszClassName=L"LocalDevAiStreamer";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    HWND hwnd=CreateWindowW(wc.lpszClassName,L"Webcam / Screen Live Streamer",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                            CW_USEDEFAULT,CW_USEDEFAULT,980,770,nullptr,nullptr,instance,nullptr);
    if(!hwnd) { CoUninitialize(); return 1; }
    ShowWindow(hwnd,show); UpdateWindow(hwnd); MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    CoUninitialize(); return static_cast<int>(message.wParam);
}
