#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include "carbon/Multiplayer.hpp"
#include "carbon/BinaryIO.hpp"

#include <nlohmann/json.hpp>
#include <array>
#include <atomic>
#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cctype>
#include <climits>
#include <functional>
#include <random>
#include <span>
#include <vector>

namespace carbon {
namespace {
using Clock = std::chrono::steady_clock;
thread_local std::function<void(const MultiplayerSnapshot&)> publishSnapshot;
constexpr int kProtocol = 777;
constexpr std::size_t kMaxPacket = 2 * 1024 * 1024;
constexpr auto kConnectTimeout = std::chrono::seconds(5);
constexpr auto kReadTimeout = std::chrono::seconds(5);

struct WinsockLifetime {
    WinsockLifetime() { WSADATA d{}; ok = WSAStartup(MAKEWORD(2, 2), &d) == 0; }
    ~WinsockLifetime() { if (ok) WSACleanup(); }
    bool ok{};
};

bool cancelled(const std::atomic_bool& flag) { return flag.load(std::memory_order_relaxed); }

void setState(MultiplayerSnapshot& out, MultiplayerState state, std::string message) {
    out.state = state;
    out.message = std::move(message);
    if (publishSnapshot) publishSnapshot(out);
}

std::vector<std::uint8_t> frame(std::int32_t packetId, std::span<const std::uint8_t> payload = {}) {
    ByteWriter body;
    body.writeVarInt(packetId);
    for (auto b : payload) body.writeByte(static_cast<std::int8_t>(b));
    ByteWriter result;
    result.writeVarInt(static_cast<std::int32_t>(body.bytes().size()));
    for (auto b : body.bytes()) result.writeByte(static_cast<std::int8_t>(b));
    return std::move(result).takeBytes();
}

std::string componentText(const nlohmann::json& j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_array()) {
        std::string s;
        for (const auto& v : j) s += componentText(v);
        return s;
    }
    if (j.is_object()) {
        std::string s;
        if (j.contains("text") && j["text"].is_string()) s = j["text"].get<std::string>();
        else if (j.contains("translate") && j["translate"].is_string()) s = j["translate"].get<std::string>();
        if (j.contains("with")) s += componentText(j["with"]);
        if (j.contains("extra")) s += componentText(j["extra"]);
        return s;
    }
    return {};
}

bool waitSocket(SOCKET s, bool writing, Clock::time_point deadline, const std::atomic_bool& stop) {
    while (!cancelled(stop) && Clock::now() < deadline) {
        fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
        timeval tv{0, 100000};
        const int r = writing ? select(0, nullptr, &fds, nullptr, &tv) : select(0, &fds, nullptr, nullptr, &tv);
        if (r > 0) return true;
        if (r == SOCKET_ERROR) return false;
    }
    return false;
}

bool sendAll(SOCKET s, std::span<const std::uint8_t> data, Clock::time_point deadline, const std::atomic_bool& stop) {
    std::size_t pos = 0;
    while (pos < data.size() && waitSocket(s, true, deadline, stop)) {
        const int n = send(s, reinterpret_cast<const char*>(data.data() + pos), static_cast<int>(std::min<std::size_t>(data.size() - pos, INT_MAX)), 0);
        if (n == SOCKET_ERROR) { if (WSAGetLastError() == WSAEWOULDBLOCK) continue; return false; }
        if (n == 0) return false;
        pos += static_cast<std::size_t>(n);
    }
    return pos == data.size();
}

bool recvExact(SOCKET s, std::span<std::uint8_t> data, Clock::time_point deadline, const std::atomic_bool& stop) {
    std::size_t pos = 0;
    while (pos < data.size() && waitSocket(s, false, deadline, stop)) {
        const int n = recv(s, reinterpret_cast<char*>(data.data() + pos), static_cast<int>(std::min<std::size_t>(data.size() - pos, INT_MAX)), 0);
        if (n == SOCKET_ERROR) { if (WSAGetLastError() == WSAEWOULDBLOCK) continue; return false; }
        if (n == 0) return false;
        pos += static_cast<std::size_t>(n);
    }
    return pos == data.size();
}

bool readVarInt(SOCKET s, std::int32_t& value, Clock::time_point deadline, const std::atomic_bool& stop) {
    std::uint32_t bits = 0;
    for (unsigned i = 0; i < 5; ++i) {
        std::uint8_t b{};
        if (!recvExact(s, std::span<std::uint8_t>(&b, 1), deadline, stop)) return false;
        bits |= std::uint32_t(b & 0x7f) << (7 * i);
        if (!(b & 0x80)) { value = std::bit_cast<std::int32_t>(bits); return true; }
    }
    return false;
}

bool readPacket(SOCKET s, std::vector<std::uint8_t>& packet, Clock::time_point deadline, const std::atomic_bool& stop) {
    std::int32_t length{};
    if (!readVarInt(s, length, deadline, stop) || length <= 0 || static_cast<std::size_t>(length) > kMaxPacket) return false;
    packet.resize(static_cast<std::size_t>(length));
    return recvExact(s, packet, deadline, stop);
}

std::vector<std::uint8_t> makeHandshake(const ServerAddress& address, std::int32_t nextState) {
    ByteWriter p; p.writeVarInt(kProtocol); p.writeUtf(address.host, 255); p.writeShort(static_cast<std::int16_t>(address.port)); p.writeVarInt(nextState);
    return frame(0, p.bytes());
}

SOCKET connectSocket(const ServerAddress& address, const std::atomic_bool& stop, MultiplayerSnapshot& out) {
    setState(out, MultiplayerState::Resolving, "Resolving server address...");
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    addrinfo* list{};
    const auto service = std::to_string(address.port);
    const int resolve = getaddrinfo(address.host.c_str(), service.c_str(), &hints, &list);
    if (resolve != 0 || !list) { setState(out, MultiplayerState::Failed, "Could not resolve server address"); return INVALID_SOCKET; }
    SOCKET connected = INVALID_SOCKET;
    for (auto* ai = list; ai && !cancelled(stop); ai = ai->ai_next) {
        setState(out, MultiplayerState::Connecting, "Connecting...");
        SOCKET s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        u_long nonblock = 1; ioctlsocket(s, FIONBIO, &nonblock);
        int r = connect(s, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
        if (r == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK && waitSocket(s, true, Clock::now() + kConnectTimeout, stop)) {
            int err = 0; int len = sizeof(err); getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
            if (err == 0) r = 0;
        }
        if (r == 0) { connected = s; break; }
        closesocket(s);
    }
    freeaddrinfo(list);
    if (connected == INVALID_SOCKET && !cancelled(stop)) setState(out, MultiplayerState::Failed, "Could not connect to server (connection timed out or refused)");
    return connected;
}

std::string loginDisconnectText(const std::vector<std::uint8_t>& packet) {
    try {
        ByteReader r(packet); if (r.readVarInt() != 0) return "";
        const auto json = nlohmann::json::parse(r.readUtf(262144));
        return componentText(json);
    } catch (...) { return "Server disconnected during login"; }
}

void runOperation(const ServerAddress& address, bool login, const std::string& username,
                  const std::atomic_bool& stop, MultiplayerSnapshot& out) {
    WinsockLifetime winsock;
    if (!winsock.ok) { setState(out, MultiplayerState::Failed, "Windows networking could not be initialized"); return; }
    SOCKET s = connectSocket(address, stop, out);
    if (s == INVALID_SOCKET) { if (cancelled(stop)) setState(out, MultiplayerState::Cancelled, "Cancelled"); return; }
    u_long nonblock = 1; ioctlsocket(s, FIONBIO, &nonblock);
    const auto ioDeadline = Clock::now() + kReadTimeout;
    const std::int32_t nextState = login ? 2 : 1;
    auto hello = makeHandshake(address, nextState);
    if (!sendAll(s, hello, ioDeadline, stop)) { if (!cancelled(stop)) setState(out, MultiplayerState::Failed, "Could not send Minecraft handshake"); closesocket(s); return; }
    if (!login) {
        auto request = frame(0);
        if (!sendAll(s, request, ioDeadline, stop)) { if (!cancelled(stop)) setState(out, MultiplayerState::Failed, "Could not send server status request"); closesocket(s); return; }
        setState(out, MultiplayerState::Status, "Requesting server status...");
        std::vector<std::uint8_t> packet;
        if (!readPacket(s, packet, ioDeadline, stop)) { if (!cancelled(stop)) setState(out, MultiplayerState::Failed, "Timed out waiting for server status"); closesocket(s); return; }
        try {
            ByteReader r(packet); if (r.readVarInt() != 0) throw BinaryIOError("unexpected status packet id");
            auto status = nlohmann::json::parse(r.readUtf(262144));
            if (status.contains("version") && status["version"].is_object()) {
                auto v = status["version"]; if (v.contains("name")) out.version = v["name"].get<std::string>();
                if (v.contains("protocol") && v["protocol"].is_number_integer()) out.protocol = v["protocol"].get<std::int32_t>();
            }
            if (status.contains("description")) out.motd = componentText(status["description"]);
            if (status.contains("players") && status["players"].is_object()) {
                auto p = status["players"];
                if (p.contains("online") && p["online"].is_number_integer()) out.playersOnline = p["online"].get<std::int32_t>();
                if (p.contains("max") && p["max"].is_number_integer()) out.playersMax = p["max"].get<std::int32_t>();
            }
        } catch (const std::exception& e) { setState(out, MultiplayerState::Failed, std::string("Invalid server status response: ") + e.what()); closesocket(s); return; }
        const auto pingStarted = Clock::now();
        const auto ping = std::chrono::duration_cast<std::chrono::milliseconds>(pingStarted.time_since_epoch()).count();
        ByteWriter pingPayload; pingPayload.writeLong(ping); auto pingFrame = frame(1, pingPayload.bytes());
        if (!sendAll(s, pingFrame, ioDeadline, stop) || !readPacket(s, packet, ioDeadline, stop)) {
            if (!cancelled(stop)) setState(out, MultiplayerState::Failed, "Server status received, but ping timed out");
            closesocket(s); return;
        }
        try {
            ByteReader r(packet); if (r.readVarInt() != 1 || r.readLong() != ping) throw BinaryIOError("invalid pong");
            out.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - pingStarted).count();
            setState(out, MultiplayerState::StatusComplete, "Server status received (Java protocol " + std::to_string(out.protocol) + ")");
        } catch (...) { setState(out, MultiplayerState::Failed, "Invalid ping response from server"); }
        closesocket(s); return;
    }

    setState(out, MultiplayerState::Login, "Starting login (offline profile; online authentication unavailable)...");
    ByteWriter helloPayload; helloPayload.writeUtf(username, 16);
    std::array<std::uint8_t, 16> uuid{};
    std::random_device random; for (auto& b : uuid) b = static_cast<std::uint8_t>(random());
    uuid[6] = static_cast<std::uint8_t>((uuid[6] & 0x0f) | 0x40); uuid[8] = static_cast<std::uint8_t>((uuid[8] & 0x3f) | 0x80);
    for (auto b : uuid) helloPayload.writeByte(static_cast<std::int8_t>(b));
    auto loginHello = frame(0, helloPayload.bytes());
    if (!sendAll(s, loginHello, ioDeadline, stop)) { if (!cancelled(stop)) setState(out, MultiplayerState::Failed, "Could not send login start"); closesocket(s); return; }
    std::vector<std::uint8_t> packet;
    if (!readPacket(s, packet, ioDeadline, stop)) { if (!cancelled(stop)) setState(out, MultiplayerState::Failed, "Timed out waiting for login response"); closesocket(s); return; }
    try {
        ByteReader r(packet); const auto id = r.readVarInt();
        if (id == 0) {
            auto reason = loginDisconnectText(packet);
            std::string lowered = reason; for (auto& c : lowered) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowered.find("auth") != std::string::npos || lowered.find("online") != std::string::npos || lowered.find("verify") != std::string::npos)
                setState(out, MultiplayerState::OnlineAuthenticationRequired, reason.empty() ? "Server requires online account authentication, which Carbon does not provide" : reason);
            else setState(out, MultiplayerState::Disconnected, reason.empty() ? "Server rejected login" : reason);
        } else if (id == 1) {
            setState(out, MultiplayerState::EncryptionRequired, "Server requires encrypted online authentication; no account credentials are available");
        } else if (id == 2) {
            // Packet id 3 is ServerboundLoginAcknowledged in the supplied 26.3
            // LoginProtocols registration order. Do not claim configuration or play.
            auto ack = frame(3);
            if (!sendAll(s, ack, ioDeadline, stop)) setState(out, MultiplayerState::Failed, "Login was accepted, but acknowledgement failed");
            else setState(out, MultiplayerState::LoginAcceptedConfigurationUnsupported, "Login accepted; 26.3 configuration and play session are not implemented yet");
        } else if (id == 3) {
            ByteReader compression(packet); compression.readVarInt(); const auto threshold = compression.readVarInt();
            setState(out, MultiplayerState::Failed, "Server enabled packet compression (threshold " + std::to_string(threshold) + "); compressed login is not implemented");
        } else if (id == 5) {
            setState(out, MultiplayerState::Failed, "Server requested a login cookie; cookie exchange is not implemented");
        } else {
            setState(out, MultiplayerState::Failed, "Unsupported packet during 26.3 login (packet id " + std::to_string(id) + ")");
        }
    } catch (const std::exception& e) { setState(out, MultiplayerState::Failed, std::string("Invalid login response: ") + e.what()); }
    closesocket(s);
}
}

std::optional<ServerAddress> ServerAddress::parse(const std::string& text, std::string* error) {
    auto fail = [&](const char* why) -> std::optional<ServerAddress> { if (error) *error = why; return std::nullopt; };
    if (text.empty() || text.size() > 300) return fail("Enter a server address");
    std::string host; std::uint16_t port = 25565;
    if (text.front() == '[') {
        auto close = text.find(']');
        if (close == std::string::npos) return fail("IPv6 addresses must use [address]:port");
        host = text.substr(1, close - 1);
        if (host.empty()) return fail("Enter a valid IPv6 address");
        if (close + 1 < text.size()) {
            if (text[close + 1] != ':') return fail("Expected :port after IPv6 address");
            unsigned parsed{}; auto first = text.data() + close + 2; auto last = text.data() + text.size();
            if (first == last || std::from_chars(first, last, parsed).ec != std::errc{} || parsed == 0 || parsed > 65535) return fail("Port must be between 1 and 65535");
            port = static_cast<std::uint16_t>(parsed);
        }
    } else {
        const auto colon = text.find(':');
        if (colon != std::string::npos && text.find(':', colon + 1) != std::string::npos) return fail("IPv6 addresses must be enclosed in brackets");
        host = colon == std::string::npos ? text : text.substr(0, colon);
        if (colon != std::string::npos) {
            unsigned parsed{}; auto first = text.data() + colon + 1; auto last = text.data() + text.size();
            if (first == last || std::from_chars(first, last, parsed).ec != std::errc{} || parsed == 0 || parsed > 65535) return fail("Port must be between 1 and 65535");
            port = static_cast<std::uint16_t>(parsed);
        }
    }
    if (host.empty() || host.find_first_of(" \t\r\n") != std::string::npos) return fail("Enter a valid host name");
    return ServerAddress{host, port, text};
}

MultiplayerClient::MultiplayerClient() = default;
MultiplayerClient::~MultiplayerClient() { cancel(); if (worker_.joinable()) worker_.join(); }
bool MultiplayerClient::startStatus(const ServerAddress& address) { start(address, false, {}); return true; }
bool MultiplayerClient::startLogin(const ServerAddress& address, const std::string& username) {
    if (username.empty() || username.size() > 16) {
        std::lock_guard lock(mutex_); snapshot_ = {}; snapshot_.state = MultiplayerState::Failed; snapshot_.message = "Username must contain 1 to 16 characters"; return false;
    }
    for (unsigned char c : username) if (!(std::isalnum(c) || c == '_')) {
        std::lock_guard lock(mutex_); snapshot_ = {}; snapshot_.state = MultiplayerState::Failed; snapshot_.message = "Username may contain only letters, digits, and underscore"; return false;
    }
    start(address, true, username); return true;
}
void MultiplayerClient::start(const ServerAddress& address, bool login, std::string username) {
    cancel(); if (worker_.joinable()) worker_.join();
    {
        std::lock_guard lock(mutex_); snapshot_ = {}; snapshot_.state = MultiplayerState::Resolving; snapshot_.message = "Starting..."; active_ = true; cancelRequested_ = false;
    }
    worker_ = std::thread([this, address, login, username = std::move(username)] {
        MultiplayerSnapshot result;
        // Access cancellation only through a local polling proxy, synchronized
        // by the worker's bounded waits. No UI thread touches socket ownership.
        struct StopFlag : std::atomic_bool {};
        // Worker loop operations use a flag copied from a tiny watcher thread
        // below so the UI can request cancellation without closing a reused fd.
        StopFlag stopFlag;
        std::thread watcher([this, &stopFlag] {
            for (;;) {
                if (stopFlag.load()) return;
                { std::lock_guard lock(mutex_); if (cancelRequested_) { stopFlag.store(true); return; } if (!active_) return; }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
        publishSnapshot = [this](const MultiplayerSnapshot& progress) {
            std::lock_guard lock(mutex_);
            if (!cancelRequested_) snapshot_ = progress;
        };
        runOperation(address, login, username, stopFlag, result);
        publishSnapshot = {};
        stopFlag.store(true);
        if (watcher.joinable()) watcher.join();
        std::lock_guard lock(mutex_);
        if (cancelRequested_) { snapshot_.state = MultiplayerState::Cancelled; snapshot_.message = "Cancelled"; }
        else snapshot_ = std::move(result);
        active_ = false;
    });
}
MultiplayerSnapshot MultiplayerClient::poll() const { std::lock_guard lock(mutex_); return snapshot_; }
void MultiplayerClient::cancel() {
    std::lock_guard lock(mutex_);
    if (active_) cancelRequested_ = true;
}

} // namespace carbon
