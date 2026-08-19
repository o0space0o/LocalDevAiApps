cmake_minimum_required(VERSION 3.20)
project(LocalDevAiAppSpace VERSION 1.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(stream_core STATIC src/stream_core.cpp src/stream_core.h)
target_compile_definitions(stream_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(stream_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(stream_core PUBLIC user32 gdi32 ole32 oleaut32 strmiids)

add_executable(video_streamer WIN32 src/main.cpp src/webcam_preview.cpp src/webcam_preview.h)
target_compile_definitions(video_streamer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(video_streamer PRIVATE stream_core comdlg32)

add_executable(stream_probe src/test.cpp)
target_compile_definitions(stream_probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(stream_probe PRIVATE stream_core)

add_executable(camera_capture_probe src/camera_capture_probe.cpp)
target_compile_definitions(camera_capture_probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(camera_capture_probe PRIVATE user32 gdi32 ole32 oleaut32 strmiids)

add_library(triage_clock_core STATIC src/triage_clock_core.cpp src/triage_clock_core.h)
target_compile_definitions(triage_clock_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(triage_clock_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(triage_clock WIN32 src/triage_clock_gui.cpp src/triage_clock_core.h)
target_compile_definitions(triage_clock PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(triage_clock PRIVATE triage_clock_core user32 gdi32)

add_executable(triage_clock_probe src/triage_clock_probe.cpp)
target_compile_definitions(triage_clock_probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(triage_clock_probe PRIVATE triage_clock_core)

add_library(space_3d_core STATIC
    src/space_3d_core.cpp src/space_3d_core.h
    src/skeleton_body.cpp src/skeleton_body.h
)
target_compile_definitions(space_3d_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(space_3d_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(space_3d_core PUBLIC user32 gdi32)

add_library(space_3d_routine STATIC
    src/space_3d_routine.cpp src/space_3d_routine.h
    src/space_3d_runtime.cpp src/space_3d_runtime.h
)
target_compile_definitions(space_3d_routine PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(space_3d_routine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(space_3d_routine PUBLIC space_3d_core triage_clock_core)

add_executable(space_3d WIN32 src/space_3d_gui.cpp)
target_compile_definitions(space_3d PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(space_3d PRIVATE space_3d_routine)

add_executable(space_3d_probe src/space_3d_probe.cpp)
target_compile_definitions(space_3d_probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(space_3d_probe PRIVATE space_3d_core)

add_executable(skeleton_body_probe src/skeleton_body_probe.cpp)
target_compile_definitions(skeleton_body_probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(skeleton_body_probe PRIVATE space_3d_core)

add_executable(space_3d_routine_probe src/space_3d_routine_probe.cpp)
target_compile_definitions(space_3d_routine_probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(space_3d_routine_probe PRIVATE space_3d_routine)

add_executable(ldai_launcher src/ldai_launcher.cpp)
target_compile_definitions(ldai_launcher PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

enable_testing()
add_test(NAME stream_probe_test COMMAND stream_probe)
set_tests_properties(stream_probe_test PROPERTIES TIMEOUT 30)
add_test(NAME space_3d_probe_test COMMAND space_3d_probe)
set_tests_properties(space_3d_probe_test PROPERTIES TIMEOUT 30)
add_test(NAME skeleton_body_probe_test COMMAND skeleton_body_probe)
set_tests_properties(skeleton_body_probe_test PROPERTIES TIMEOUT 30)
add_test(NAME triage_clock_probe_test COMMAND triage_clock_probe)
set_tests_properties(triage_clock_probe_test PROPERTIES TIMEOUT 30)
add_test(NAME space_3d_routine_probe_test COMMAND space_3d_routine_probe)
set_tests_properties(space_3d_routine_probe_test PROPERTIES TIMEOUT 30)
add_test(NAME ldai_launcher_test COMMAND ldai_launcher)
set_tests_properties(ldai_launcher_test PROPERTIES TIMEOUT 30)
