#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace carbon {

struct ServerAddress {
    std::string host;
    std::uint16_t port{25565};
    std::string display;
    static std::optional<ServerAddress> parse(const std::string& text, std::string* error = nullptr);
};

enum class MultiplayerState {
    Idle, Resolving, Connecting, Status, StatusComplete, Login,
    EncryptionRequired, OnlineAuthenticationRequired,
    LoginAcceptedConfigurationUnsupported, Disconnected,
    Failed, Cancelled
};

struct MultiplayerSnapshot {
    MultiplayerState state{MultiplayerState::Idle};
    std::string message;
    std::string motd;
    std::string version;
    std::int32_t protocol{-1};
    std::int32_t playersOnline{-1};
    std::int32_t playersMax{-1};
    std::int64_t latencyMs{-1};
};

// Owns one bounded, cancellable network operation. Poll from the UI thread; do
// not call startStatus/startLogin again until the prior operation has finished.
class MultiplayerClient {
public:
    MultiplayerClient();
    ~MultiplayerClient();
    MultiplayerClient(const MultiplayerClient&) = delete;
    MultiplayerClient& operator=(const MultiplayerClient&) = delete;

    bool startStatus(const ServerAddress& address);
    bool startLogin(const ServerAddress& address, const std::string& username);
    MultiplayerSnapshot poll() const;
    void cancel();

private:
    void start(const ServerAddress& address, bool login, std::string username);
    mutable std::mutex mutex_;
    MultiplayerSnapshot snapshot_;
    std::thread worker_;
    bool active_{};
    bool cancelRequested_{};
};

} // namespace carbon
