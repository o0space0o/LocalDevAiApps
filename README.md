# Webcam / Screen Live Streamer & 3D Space Simulator

A local development environment workspace with multiple Windows ARM64 applications:

## Applications

### Video Streamer

A small native Windows application that previews the desktop or a webcam and sends that source to a YouTube Live or Facebook Live RTMP/RTMPS ingest endpoint through FFmpeg.

**Features:**
- Live desktop preview using Windows GDI capture.
- Webcam discovery and preview using Windows DirectShow.
- Screen or webcam streaming through a separately installed `ffmpeg.exe`.
- Server URL and private stream-key fields for YouTube Live, Facebook Live, or another RTMP service.
- Visible OFFLINE / CONNECTING / PROCESS RUNNING / ERROR status.
- Stream keys are masked in the UI and redacted from diagnostic command generation.
- No administrator access, background persistence, account login, or bundled credentials.

### 3D Space - Ball Physics Simulator

An interactive 3D visualization application with physics simulation and ball tracking.

**Features:**
- 3D coordinate axis display (X, Y, Z) with AutoCAD-style navigation
- Real-time ball physics simulation with gravity and collision detection
- Interactive ball-throwing (middle-click in viewport)
- Advanced volume and density calculations for each ball
- Ball tracking with real-time position and velocity display
- Rotatable camera view using left-mouse drag
- Visual grid for spatial reference
- Multi-ball collision resolution
- Boundary collision and bounce damping

## Build (Windows ARM64)

Use the Visual Studio ARM64 developer environment:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The output applications are:

- `video_streamer.exe` — interactive viewer and streamer.
- `stream_probe.exe` — noninteractive desktop-capture and RTMP-command self-test.
- `space_3d.exe` — interactive 3D ball physics simulator with visualization.
- `space_3d_probe.exe` — noninteractive 3D engine validation probe.

## Video Streaming

1. Install an ARM64-compatible FFmpeg build that includes `gdigrab`, `dshow`, `libx264`, AAC, and FLV support.
2. In YouTube Studio or Facebook Live Producer, create a live stream and copy its **server URL** and **stream key**.
3. Start `video_streamer.exe`, select the desktop or a webcam, paste the URL and key, and choose `ffmpeg.exe`.
4. Press **Start live stream**. The app reports whether the FFmpeg process is running; confirm the incoming preview and final LIVE state in the provider dashboard.

The stream key is a secret. Do not commit it, paste it into logs, or share it. This application does not call account APIs or store keys; it uses the provider-issued RTMP destination for the current session.

## 3D Space Usage

1. Start `space_3d.exe`
2. **Add Ball** button: Adds a ball with calculated physics
3. **Clear All** button: Removes all balls from the scene
4. **Left-mouse drag**: Rotates the 3D camera view
5. **Middle-click in viewport**: Throws a ball at the clicked position
6. **Delete key**: Clears all balls
7. Info panel shows ball count and real-time statistics (position, volume, velocity)

## Probes

- `stream_probe.exe` captures a real desktop frame, writes `stream_probe.bmp` beside the executable, validates screen/webcam FFmpeg command construction, verifies secret redaction, and prints `STREAM_PROBE_OK` on success. It does not contact a streaming provider or require a real stream key.

- `space_3d_probe.exe` validates the physics engine with ball creation, volume/density calculations, physics updates, camera rotation, and clearing operations. Prints `SPACE3D_PROBE_OK` on success.
