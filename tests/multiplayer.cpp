// Native integration against a handwritten, source-derived loopback server
// fixture. This validates Carbon framing, fragmentation tolerance, address
// parsing, and status/ping flow; it is not execution of Minecraft or a vanilla
// server parity test.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include "carbon/BinaryIO.hpp"
#include "carbon/Multiplayer.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

static bool recvExact(SOCKET s, char* b, int n) {
    int p = 0; while (p < n) { int r = recv(s, b + p, n - p, 0); if (r <= 0) return false; p += r; } return true;
}
static bool readVarInt(SOCKET s, std::int32_t& out) {
    std::uint32_t bits = 0;
    for (int i = 0; i < 5; ++i) { unsigned char b{}; if (!recvExact(s, reinterpret_cast<char*>(&b), 1)) return false; bits |= std::uint32_t(b & 127) << (7 * i); if (!(b & 128)) { out = std::bit_cast<std::int32_t>(bits); return true; } }
    return false;
}
static bool readFrame(SOCKET s, std::vector<std::uint8_t>& p) {
    std::int32_t n{}; if (!readVarInt(s, n) || n < 1 || n > 2 * 1024 * 1024) return false;
    p.resize(static_cast<std::size_t>(n)); return recvExact(s, reinterpret_cast<char*>(p.data()), n);
}
static bool sendAll(SOCKET s, const std::vector<std::uint8_t>& b) {
    std::size_t p = 0; while (p < b.size()) { int n = send(s, reinterpret_cast<const char*>(b.data() + p), static_cast<int>(b.size() - p), 0); if (n <= 0) return false; p += static_cast<std::size_t>(n); } return true;
}
static std::vector<std::uint8_t> packet(std::int32_t id, const std::vector<std::uint8_t>& payload = {}) {
    carbon::ByteWriter body; body.writeVarInt(id); for (auto b : payload) body.writeByte(static_cast<std::int8_t>(b));
    carbon::ByteWriter frame; frame.writeVarInt(static_cast<std::int32_t>(body.bytes().size())); for (auto b : body.bytes()) frame.writeByte(static_cast<std::int8_t>(b));
    return std::move(frame).takeBytes();
}
static bool readServerPacket(SOCKET s, std::vector<std::uint8_t>& p, std::int32_t expectedId) {
    if (!readFrame(s, p)) return false; carbon::ByteReader r(p); return r.readVarInt() == expectedId;
}

int main() {
    std::string error;
    auto a = carbon::ServerAddress::parse("example.org", &error);
    assert(a && a->host == "example.org" && a->port == 25565);
    a = carbon::ServerAddress::parse("[::1]:25566", &error);
    assert(a && a->host == "::1" && a->port == 25566);
    assert(!carbon::ServerAddress::parse("::1", &error));
    assert(!carbon::ServerAddress::parse("host:65536", &error));

    WSADATA wsa{}; assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); assert(listener != INVALID_SOCKET);
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_addr.s_addr = htonl(INADDR_LOOPBACK); local.sin_port = 0;
    assert(bind(listener, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == 0);
    assert(listen(listener, 1) == 0);
    int localLen = sizeof(local); assert(getsockname(listener, reinterpret_cast<sockaddr*>(&local), &localLen) == 0);
    const auto port = ntohs(local.sin_port);

    std::thread fixture([&] {
        SOCKET peer = accept(listener, nullptr, nullptr); assert(peer != INVALID_SOCKET);
        std::vector<std::uint8_t> p; assert(readFrame(peer, p));
        carbon::ByteReader handshake(p); assert(handshake.readVarInt() == 0); assert(handshake.readVarInt() == 777);
        assert(handshake.readUtf(255) == "127.0.0.1"); assert(static_cast<std::uint16_t>(handshake.readShort()) == port); assert(handshake.readVarInt() == 1);
        assert(readServerPacket(peer, p, 0)); // Status request
        carbon::ByteWriter payload;
        payload.writeUtf(R"({"version":{"name":"26.3","protocol":777},"players":{"max":12,"online":3},"description":{"text":"local fixture"}})");
        assert(sendAll(peer, packet(0, payload.bytes())));
        assert(readServerPacket(peer, p, 1)); // Ping request
        carbon::ByteReader ping(p); assert(ping.readVarInt() == 1); carbon::ByteWriter pongPayload; pongPayload.writeLong(ping.readLong());
        auto pong = packet(1, pongPayload.bytes());
        // Split writes to exercise TCP packet fragmentation handling.
        assert(send(peer, reinterpret_cast<const char*>(pong.data()), 1, 0) == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        assert(sendAll(peer, std::vector<std::uint8_t>(pong.begin() + 1, pong.end())));
        closesocket(peer);
    });

    auto server = carbon::ServerAddress::parse("127.0.0.1:" + std::to_string(port)); assert(server);
    carbon::MultiplayerClient client; assert(client.startStatus(*server));
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    carbon::MultiplayerSnapshot snapshot;
    do { snapshot = client.poll(); if (snapshot.state == carbon::MultiplayerState::StatusComplete || snapshot.state == carbon::MultiplayerState::Failed) break; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    while (std::chrono::steady_clock::now() < end);
    assert(snapshot.state == carbon::MultiplayerState::StatusComplete);
    assert(snapshot.protocol == 777 && snapshot.version == "26.3");
    assert(snapshot.playersOnline == 3 && snapshot.playersMax == 12);
    assert(snapshot.motd == "local fixture" && snapshot.latencyMs >= 0);
    fixture.join(); closesocket(listener); WSACleanup();
}
