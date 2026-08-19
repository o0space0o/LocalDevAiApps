#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "skeleton_body.h"

namespace skeletonnet {

constexpr std::uint32_t ProtocolVersion = 1;
constexpr std::uint16_t DefaultPort = 3000;

struct Endpoint {
    std::string host = "127.0.0.1";
    std::uint16_t port = DefaultPort;
};

bool ParseEndpoint(const std::string& url, Endpoint& endpoint, std::string& error);

struct SkeletonFrame {
    std::string source_id;
    std::string display_name;
    std::uint64_t sequence = 0;
    std::array<space3d::Vector3, space3d::HumanoidJointCount> joints{};
};

SkeletonFrame MakeFrame(const std::string& source_id, const std::string& display_name,
                        std::uint64_t sequence, const space3d::HumanoidPose& pose);

class SkeletonServer {
public:
    SkeletonServer();
    ~SkeletonServer();
    SkeletonServer(const SkeletonServer&) = delete;
    SkeletonServer& operator=(const SkeletonServer&) = delete;

    bool Start(std::uint16_t port, std::string& error);
    void Stop();
    bool IsRunning() const noexcept;
    std::uint16_t Port() const noexcept;
    std::size_t ClientCount() const;
    bool Publish(const SkeletonFrame& frame, std::string& error);
    std::vector<SkeletonFrame> Frames() const;

private:
    struct Peer;
    void AcceptLoop();
    void ReceiveLoop(const std::shared_ptr<Peer>& peer);
    bool Broadcast(const SkeletonFrame& frame, std::string& error);

    std::shared_ptr<void> winsock_;
    std::atomic<bool> running_{false};
    std::uintptr_t listen_socket_ = ~std::uintptr_t{0};
    std::uint16_t port_ = 0;
    std::thread accept_thread_;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Peer>> peers_;
    std::unordered_map<std::string, SkeletonFrame> frames_;
};

class SkeletonClient {
public:
    SkeletonClient();
    ~SkeletonClient();
    SkeletonClient(const SkeletonClient&) = delete;
    SkeletonClient& operator=(const SkeletonClient&) = delete;

    bool Connect(const std::string& backend_url, std::string& error);
    void Disconnect();
    bool IsConnected() const noexcept;
    bool Publish(const SkeletonFrame& frame, std::string& error);
    std::vector<SkeletonFrame> Frames() const;
    std::string LastError() const;

private:
    void ReceiveLoop();

    std::shared_ptr<void> winsock_;
    std::atomic<bool> connected_{false};
    std::uintptr_t socket_ = ~std::uintptr_t{0};
    std::thread receive_thread_;
    mutable std::mutex send_mutex_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SkeletonFrame> frames_;
    std::string last_error_;
};

} // namespace skeletonnet
