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
- ldai_launcher (new: app catalog)
- space_3d_scene_verify_probe (new: scene serialization verification)

Key reusable components
- space_3d_core: 3D engine, physics, serialization (Serialize/Deserialize)
- triage_clock_core: plan/timer logic
- stream_core: video/capture helper

Recent research sources
- Windows on Arm overview — https://learn.microsoft.com/en-us/windows/arm/overview
- Windows ML (Windows.AI) — https://learn.microsoft.com/en-us/windows/ai/windows-ml/
- ONNX Runtime overview — https://onnxruntime.ai/docs/
- OpenAI Agents guidance — https://developers.openai.com/api/docs/guides/agents

Completed milestones

### Milestone 1: App Catalog / Lightweight Launcher (completed in prior run)
- User value: Fast, discoverable list of installed workspace apps and metadata; first step toward a launcher UI and catalog.
- Implementation: src/ldai_launcher.cpp
- Test: noninteractive probe checks presence of space_3d.exe and triage_clock.exe in build folder and prints LDAI_LAUNCHER_OK.
- Status: ✓ Passed configure/build/CTest on native Windows ARM64. Launcher prints exact success marker.

### Milestone 2: Scene Document Verification & Round-Trip Test (completed this run)
- User value: Reproducible scene sharing and validated round-trips for space_3d documents; proves serialization is deterministic.
- Implementation: src/space_3d_scene_verify_probe.cpp (new)
- Changes: Added probe to CMakeLists.txt as space_3d_scene_verify_probe executable and test target.
- Test: Creates 3 test balls, serializes scene, deserializes into fresh engine, verifies ball count/properties match, re-serializes and validates determinism, prints SPACE_3D_SCENE_VERIFY_OK on success.
- Status: ✓ Passed fresh configure/build/CTest on native Windows ARM64. Probe prints exact success marker. Ball properties (position, velocity, radius, mass) verified within epsilon tolerance.

Next milestones (priority order)
1. Launcher UI with manifest parsing and app metadata (reuse ldai_launcher core + add WIN32 GUI panel).
2. Optional on-device ML baseline (requires separate approval and dependency plan).
3. Shared settings/storage service for cross-app configuration.

Risks & approvals
- Any ML model downloads or runtime additions require explicit user approval.
- Windows AppControl policy may block unsigned binaries; if that happens, preserve evidence and report the external blocker.

Research sources (short)
- https://learn.microsoft.com/en-us/windows/arm/overview
- https://learn.microsoft.com/en-us/windows/ai/windows-ml/
- https://onnxruntime.ai/docs/
- https://developers.openai.com/api/docs/guides/agents

End of milestone record.
