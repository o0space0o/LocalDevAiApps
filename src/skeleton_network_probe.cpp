#include "skeleton_network.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

namespace {
bool WaitFor(const auto& predicate, int milliseconds = 4000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return predicate();
}
space3d::HumanoidPose PoseAt(float x) {
    space3d::HumanoidController body;
    body.Reset({x, -5.0f, 0.0f});
    return body.BuildPose();
}
bool Has(const std::vector<skeletonnet::SkeletonFrame>& frames, const std::string& id, float x) {
    for (const auto& frame : frames) {
        if (frame.source_id == id && std::abs(frame.joints[0].x - x) < 0.01f) return true;
    }
    return false;
}
}

int main() {
    skeletonnet::Endpoint endpoint;
    std::string error;
    if (!skeletonnet::ParseEndpoint("ws://backend.o0space0o.app:3000/live", endpoint, error) ||
        endpoint.host != "backend.o0space0o.app" || endpoint.port != 3000) {
        std::cerr << "endpoint parse failed: " << error << '\n'; return 1;
    }

    skeletonnet::SkeletonServer server;
    if (!server.Start(0, error) || server.Port() == 0) {
        std::cerr << "server start failed: " << error << '\n'; return 2;
    }
    const std::string url = "tcp://127.0.0.1:" + std::to_string(server.Port());
    skeletonnet::SkeletonClient alice, bob;
    if (!alice.Connect(url, error)) { std::cerr << "alice connect: " << error << '\n'; return 3; }
    if (!bob.Connect(url, error)) { std::cerr << "bob connect: " << error << '\n'; return 4; }
    if (!WaitFor([&]{ return server.ClientCount() == 2; })) { std::cerr << "clients not accepted\n"; return 5; }

    if (!alice.Publish(skeletonnet::MakeFrame("alice", "Alice", 1, PoseAt(1.0f)), error) ||
        !bob.Publish(skeletonnet::MakeFrame("bob", "Bob", 1, PoseAt(2.0f)), error) ||
        !server.Publish(skeletonnet::MakeFrame("server", "Server Body", 1, PoseAt(3.0f)), error)) {
        std::cerr << "publish failed: " << error << '\n'; return 6;
    }
    const bool delivered = WaitFor([&]{
        const auto a = alice.Frames(); const auto b = bob.Frames(); const auto s = server.Frames();
        return Has(a,"alice",1) && Has(a,"bob",2) && Has(a,"server",3) &&
               Has(b,"alice",1) && Has(b,"bob",2) && Has(b,"server",3) &&
               Has(s,"alice",1) && Has(s,"bob",2) && Has(s,"server",3);
    });
    alice.Disconnect(); bob.Disconnect(); server.Stop();
    if (!delivered) { std::cerr << "live skeleton fan-out/integration failed\n"; return 7; }
    std::cout << "SKELLETON_NETWORK_PROBE_OK\n";
    return 0;
}