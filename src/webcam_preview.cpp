#define WIN32_LEAN_AND_MEAN
#include "webcam_preview.h"
#include <windows.h>
#include <dshow.h>

WebcamPreview::WebcamPreview() : graph_(nullptr), capture_(nullptr), control_(nullptr), video_(nullptr) {}
WebcamPreview::~WebcamPreview() { Stop(); }

bool WebcamPreview::Start(void* owner_window, const std::wstring& device_name, std::wstring& error) {
    Stop();
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IGraphBuilder, reinterpret_cast<void**>(&graph_));
    if (FAILED(hr)) { error = L"Could not create the webcam preview graph."; Stop(); return false; }
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ICaptureGraphBuilder2, reinterpret_cast<void**>(&capture_));
    if (FAILED(hr) || FAILED(capture_->SetFiltergraph(graph_))) {
        error = L"Could not create the webcam capture graph."; Stop(); return false;
    }

    ICreateDevEnum* devices = nullptr;
    IEnumMoniker* monikers = nullptr;
    IBaseFilter* source = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ICreateDevEnum, reinterpret_cast<void**>(&devices));
    if (SUCCEEDED(hr) && devices->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &monikers, 0) == S_OK) {
        IMoniker* moniker = nullptr;
        while (!source && monikers->Next(1, &moniker, nullptr) == S_OK) {
            IPropertyBag* bag = nullptr;
            VARIANT value; VariantInit(&value);
            if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, reinterpret_cast<void**>(&bag)))) {
                if (SUCCEEDED(bag->Read(L"FriendlyName", &value, nullptr)) && value.vt == VT_BSTR
                    && device_name == value.bstrVal) {
                    moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, reinterpret_cast<void**>(&source));
                }
                VariantClear(&value);
                bag->Release();
            }
            moniker->Release();
        }
    }
    if (monikers) monikers->Release();
    if (devices) devices->Release();
    if (!source) { error = L"The selected webcam is no longer available."; Stop(); return false; }

    hr = graph_->AddFilter(source, L"Webcam");
    if (SUCCEEDED(hr)) {
        hr = capture_->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, source, nullptr, nullptr);
        if (FAILED(hr)) hr = capture_->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, source, nullptr, nullptr);
    }
    source->Release();
    if (FAILED(hr)) { error = L"Windows could not render the selected webcam."; Stop(); return false; }

    hr = graph_->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&control_));
    if (SUCCEEDED(hr)) hr = graph_->QueryInterface(IID_IVideoWindow, reinterpret_cast<void**>(&video_));
    if (FAILED(hr) || !video_) { error = L"The webcam has no compatible preview renderer."; Stop(); return false; }
    video_->put_Owner(reinterpret_cast<OAHWND>(owner_window));
    video_->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
    video_->put_Visible(OATRUE);
    hr = control_->Run();
    if (FAILED(hr)) { error = L"The webcam preview could not start."; Stop(); return false; }
    return true;
}

void WebcamPreview::Resize(int x, int y, int width, int height) {
    if (video_) video_->SetWindowPosition(x, y, width, height);
}

void WebcamPreview::Stop() {
    if (control_) control_->Stop();
    if (video_) {
        video_->put_Visible(OAFALSE);
        video_->put_Owner(0);
        video_->Release(); video_ = nullptr;
    }
    if (control_) { control_->Release(); control_ = nullptr; }
    if (capture_) { capture_->Release(); capture_ = nullptr; }
    if (graph_) { graph_->Release(); graph_ = nullptr; }
}

bool WebcamPreview::Running() const { return control_ != nullptr; }
