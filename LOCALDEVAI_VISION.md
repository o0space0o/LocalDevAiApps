# LOCALDEVAI_VISION

Vision: Grow this workspace into a Windows user-space app environment of small, independently runnable applications with a simple launcher, app catalog, shared settings, and a local-AI coordinator. Preserve app independence, avoid kernel/driver changes, admin rights, or background services.

Boundaries
- Windows user-space only; no drivers, bootloaders, or admin-required changes.
- Offline-first; no telemetry, no cloud keys, no account requirements.

Inventory (scanned source)
- video_streamer (WIN32 GUI)
- stream_probe (console probe)
- camera_capture_probe (console)
- space_3d (WIN32 GUI) + space_3d_core (engine) + space_3d_probe
- skeleton_body_probe
- triage_clock (WIN32 GUI) + triage_clock_core + triage_clock_probe
- webcam_preview, stream_core utilities

Key reusable components
- space_3d_core: 3D engine, physics, serialization
- triage_clock_core: plan/timer logic
- stream_core: video/capture helper

Recent research sources
- Windows on Arm overview — https://learn.microsoft.com/en-us/windows/arm/overview
- Windows ML (Windows.AI) — https://learn.microsoft.com/en-us/windows/ai/windows-ml/
- ONNX Runtime overview — https://onnxruntime.ai/docs/
- OpenAI Agents guidance — https://developers.openai.com/api/docs/guides/agents

Three proposed milestones (from local-AI critique)
1) App Catalog / Lightweight Launcher (chosen)
   - User value: Fast, discoverable list of installed workspace apps and metadata; first step toward a launcher UI and catalog.
   - Touched files: CMakeLists.txt (add target), new src/ldai_launcher.cpp, LOCALDEVAI_VISION.md (update).
   - ARM64 feasibility: trivial (C++17 filesystem + console app). Build/testable with existing toolchain.
   - Test: noninteractive probe checks presence of space_3d.exe and triage_clock.exe in build folder and prints LDAI_LAUNCHER_OK.
   - Risks: low. No admin or runtime deps. No approval required.

2) Save/Load Scene Interop & Versioned Documents
   - User value: Reproducible scene sharing and validated round-trips for 3d_space documents.
   - Touched files: space_3d_core serialization tests, UI save/load handlers, sample scenes.
   - ARM64 feasibility: medium (already has Serialize/Deserialize). Requires careful test data.
   - Test: round-trip save/load of serialized document and consistency checks.
   - Risks: moderate (file parsing edge cases). No special approval.

3) Local ML Inference Baseline (on-device)
   - User value: Add an offline ML capability for simple classification (e.g., scene tagger) using ONNX Runtime or Windows ML.
   - Touched files: new ml/ folder, CMake entries, optional provider integration.
   - ARM64 feasibility: depends on installed ONNX Runtime provider; preliminary research required.
   - Test: run a tiny ONNX model inference CPU-only with deterministic output.
   - Risks: higher — binary dependencies, model download, and licensing; requires explicit user approval before fetching models or adding heavy deps.

Decision: implement milestone #1 (App Catalog / Lightweight Launcher).
Rationale: smallest useful step, low risk, immediately testable on ARM64 with current toolchain, and provides infrastructure reused by later launcher UI and catalog features.

Completed action in this run
- Added a new console executable `ldai_launcher` that inspects the ARM64 build output folder for known workspace apps and validates they exist. The probe prints `LDAI_LAUNCHER_OK` on success.

Next milestones (priority order)
1. Launcher UI with manifest parsing and app metadata (after this catalog exists).
2. Scene document library and verification tests for space_3d.
3. Optional on-device ML baseline (requires separate approval and dependency plan).

Risks & approvals
- Any ML model downloads or runtime additions require explicit user approval.
- Windows AppControl policy may block unsigned binaries; if that happens, preserve evidence and report the external blocker.

Research sources (short)
- https://learn.microsoft.com/en-us/windows/arm/overview
- https://learn.microsoft.com/en-us/windows/ai/windows-ml/
- https://onnxruntime.ai/docs/
- https://developers.openai.com/api/docs/guides/agents

End of change.
