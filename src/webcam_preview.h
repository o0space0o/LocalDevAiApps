#pragma once
#include <objbase.h>
#include <string>

struct IGraphBuilder;
struct ICaptureGraphBuilder2;
struct IMediaControl;
struct IVideoWindow;

class WebcamPreview {
public:
    WebcamPreview();
    ~WebcamPreview();
    bool Start(void* owner_window, const std::wstring& device_name, std::wstring& error);
    void Resize(int x, int y, int width, int height);
    void Stop();
    bool Running() const;
private:
    IGraphBuilder* graph_;
    ICaptureGraphBuilder2* capture_;
    IMediaControl* control_;
    IVideoWindow* video_;
};
