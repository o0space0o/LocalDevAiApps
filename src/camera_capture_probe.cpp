#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dshow.h>
#include <objbase.h>
#include <oleauto.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

struct ISampleGrabberCB;

MIDL_INTERFACE("6B652FFF-11FE-4FCE-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL one_shot) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* type) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE* type) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL buffer_them) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* size, long* buffer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample** sample) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB* callback, long method) = 0;
};

static const CLSID CLSID_SampleGrabberLocal =
    {0xC1F400A0, 0x3F08, 0x11D3, {0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37}};
static const CLSID CLSID_NullRendererLocal =
    {0xC1F400A4, 0x3F08, 0x11D3, {0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37}};

namespace {

template <class T> void Release(T*& value) {
    if (value) { value->Release(); value = nullptr; }
}

void FreeMediaType(AM_MEDIA_TYPE& type) {
    if (type.cbFormat != 0) {
        CoTaskMemFree(type.pbFormat);
        type.cbFormat = 0;
        type.pbFormat = nullptr;
    }
    if (type.pUnk) {
        type.pUnk->Release();
        type.pUnk = nullptr;
    }
}

std::wstring HrText(HRESULT hr) {
    wchar_t* text = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), 0,
                   reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring result = text ? text : L"unknown error";
    if (text) LocalFree(text);
    return result;
}

int Fail(const std::wstring& message, HRESULT hr = S_OK) {
    std::wcerr << L"CAMERA_CAPTURE_PROBE_FAIL: " << message;
    if (FAILED(hr)) std::wcerr << L" (0x" << std::hex << static_cast<unsigned long>(hr)
                               << L": " << HrText(hr) << L")";
    std::wcerr << L"\n";
    return 1;
}

bool SaveBmp(const std::wstring& path, int width, int height,
             int source_stride, const std::vector<std::uint8_t>& source) {
    const int absolute_height = std::abs(height);
    const int output_stride = (width * 3 + 3) & ~3;
    if (width <= 0 || absolute_height <= 0 || source_stride < width * 3 ||
        source.size() < static_cast<size_t>(source_stride) * absolute_height) return false;

    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER info_header{};
    info_header.biSize = sizeof(info_header);
    info_header.biWidth = width;
    info_header.biHeight = absolute_height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = output_stride * absolute_height;
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + info_header.biSizeImage;

    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    output.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
    std::vector<std::uint8_t> row(output_stride, 0);
    for (int y = 0; y < absolute_height; ++y) {
        // DirectShow RGB frames are normally bottom-up for positive heights. Normalize
        // top-down formats into the bottom-up BMP layout expected by this header.
        const int source_y = height > 0 ? y : (absolute_height - 1 - y);
        std::copy_n(source.data() + static_cast<size_t>(source_y) * source_stride,
                    width * 3, row.data());
        output.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    return output.good();
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    const std::wstring output_path = argc > 1 ? argv[1] : L"camera_probe.bmp";
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return Fail(L"COM initialization failed", hr);

    ICreateDevEnum* device_enum = nullptr;
    IEnumMoniker* monikers = nullptr;
    IMoniker* moniker = nullptr;
    IBaseFilter* camera = nullptr;
    IGraphBuilder* graph = nullptr;
    ICaptureGraphBuilder2* capture = nullptr;
    IBaseFilter* grabber_filter = nullptr;
    ISampleGrabber* grabber = nullptr;
    IBaseFilter* null_renderer = nullptr;
    IMediaControl* control = nullptr;
    AM_MEDIA_TYPE connected{};
    std::wstring camera_name = L"unnamed camera";
    int result = 1;

    do {
        hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                              IID_ICreateDevEnum, reinterpret_cast<void**>(&device_enum));
        if (FAILED(hr)) { result = Fail(L"Could not create the video-device enumerator", hr); break; }
        hr = device_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &monikers, 0);
        if (hr != S_OK) { result = Fail(L"No Windows video capture device is available", hr); break; }
        hr = monikers->Next(1, &moniker, nullptr);
        if (hr != S_OK) { result = Fail(L"No Windows video capture device could be selected", hr); break; }

        IPropertyBag* bag = nullptr;
        if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                             reinterpret_cast<void**>(&bag)))) {
            VARIANT name; VariantInit(&name);
            if (SUCCEEDED(bag->Read(L"FriendlyName", &name, nullptr)) && name.vt == VT_BSTR)
                camera_name = name.bstrVal;
            VariantClear(&name);
            bag->Release();
        }
        hr = moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter,
                                   reinterpret_cast<void**>(&camera));
        if (FAILED(hr)) { result = Fail(L"The camera could not be opened (it may be busy or permission-blocked)", hr); break; }

        hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IGraphBuilder, reinterpret_cast<void**>(&graph));
        if (FAILED(hr)) { result = Fail(L"Could not create the capture graph", hr); break; }
        hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                              IID_ICaptureGraphBuilder2, reinterpret_cast<void**>(&capture));
        if (FAILED(hr) || FAILED(hr = capture->SetFiltergraph(graph))) {
            result = Fail(L"Could not configure the capture graph", hr); break;
        }
        if (FAILED(hr = graph->AddFilter(camera, L"Camera"))) {
            result = Fail(L"Could not add the camera to the capture graph", hr); break;
        }

        hr = CoCreateInstance(CLSID_SampleGrabberLocal, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IBaseFilter, reinterpret_cast<void**>(&grabber_filter));
        if (FAILED(hr)) { result = Fail(L"The Windows DirectShow sample grabber is unavailable", hr); break; }
        if (FAILED(hr = grabber_filter->QueryInterface(__uuidof(ISampleGrabber),
                                                       reinterpret_cast<void**>(&grabber)))) {
            result = Fail(L"Could not access the sample grabber", hr); break;
        }
        AM_MEDIA_TYPE requested{};
        requested.majortype = MEDIATYPE_Video;
        requested.subtype = MEDIASUBTYPE_RGB24;
        requested.formattype = FORMAT_VideoInfo;
        if (FAILED(hr = grabber->SetMediaType(&requested)) ||
            FAILED(hr = grabber->SetBufferSamples(TRUE)) ||
            FAILED(hr = grabber->SetOneShot(FALSE)) ||
            FAILED(hr = graph->AddFilter(grabber_filter, L"Frame Grabber"))) {
            result = Fail(L"Could not configure frame capture", hr); break;
        }

        hr = CoCreateInstance(CLSID_NullRendererLocal, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IBaseFilter, reinterpret_cast<void**>(&null_renderer));
        if (FAILED(hr) || FAILED(hr = graph->AddFilter(null_renderer, L"Null Renderer"))) {
            result = Fail(L"Could not configure the capture sink", hr); break;
        }
        hr = capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                   camera, grabber_filter, null_renderer);
        if (FAILED(hr)) {
            hr = capture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video,
                                       camera, grabber_filter, null_renderer);
        }
        if (FAILED(hr)) { result = Fail(L"The camera has no compatible RGB capture format", hr); break; }
        if (FAILED(hr = grabber->GetConnectedMediaType(&connected)) ||
            connected.formattype != FORMAT_VideoInfo || !connected.pbFormat) {
            result = Fail(L"Could not read the negotiated camera format", hr); break;
        }
        const auto* video = reinterpret_cast<const VIDEOINFOHEADER*>(connected.pbFormat);
        const int width = video->bmiHeader.biWidth;
        const int height = video->bmiHeader.biHeight;
        const int absolute_height = std::abs(height);
        const int stride = ((width * 24 + 31) / 32) * 4;
        if (width < 16 || absolute_height < 16) {
            result = Fail(L"The camera negotiated invalid frame dimensions"); break;
        }

        if (FAILED(hr = graph->QueryInterface(IID_IMediaControl,
                                              reinterpret_cast<void**>(&control))) ||
            FAILED(hr = control->Run())) {
            result = Fail(L"The camera capture stream could not start", hr); break;
        }

        long byte_count = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
        while (std::chrono::steady_clock::now() < deadline) {
            hr = grabber->GetCurrentBuffer(&byte_count, nullptr);
            if (SUCCEEDED(hr) && byte_count >= stride * absolute_height) break;
            byte_count = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (byte_count <= 0) { result = Fail(L"The camera opened but did not deliver a frame before timeout", hr); break; }
        std::vector<std::uint8_t> pixels(static_cast<size_t>(byte_count));
        if (FAILED(hr = grabber->GetCurrentBuffer(&byte_count,
                                                  reinterpret_cast<long*>(pixels.data())))) {
            result = Fail(L"Could not copy the camera frame", hr); break;
        }

        std::uint8_t minimum = 255, maximum = 0;
        std::uint64_t luma_sum = 0;
        std::uint64_t non_dark = 0;
        const size_t pixel_count = static_cast<size_t>(width) * absolute_height;
        for (int y = 0; y < absolute_height; ++y) {
            const auto* row = pixels.data() + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const auto* p = row + x * 3;
                const std::uint8_t luma = static_cast<std::uint8_t>((p[0] * 29 + p[1] * 150 + p[2] * 77) >> 8);
                minimum = std::min(minimum, luma);
                maximum = std::max(maximum, luma);
                luma_sum += luma;
                if (luma > 8) ++non_dark;
            }
        }
        const double average = pixel_count ? static_cast<double>(luma_sum) / pixel_count : 0.0;
        const double non_dark_percent = pixel_count ? 100.0 * non_dark / pixel_count : 0.0;
        if (maximum - minimum < 3 || average < 1.0 || non_dark_percent < 1.0) {
            result = Fail(L"The camera delivered an unusable black or constant frame"); break;
        }
        if (!SaveBmp(output_path, width, height, stride, pixels)) {
            result = Fail(L"Could not save the captured camera frame"); break;
        }

        std::wcout << L"CAMERA_CAPTURE_PROBE_OK device=\"" << camera_name << L"\" size="
                   << width << L"x" << absolute_height << L" average_luma=" << average
                   << L" non_dark_percent=" << non_dark_percent << L" output=\""
                   << output_path << L"\"\n";
        result = 0;
    } while (false);

    if (control) control->Stop();
    FreeMediaType(connected);
    Release(control);
    Release(null_renderer);
    Release(grabber);
    Release(grabber_filter);
    Release(capture);
    Release(graph);
    Release(camera);
    Release(moniker);
    Release(monikers);
    Release(device_enum);
    CoUninitialize();
    return result;
}
