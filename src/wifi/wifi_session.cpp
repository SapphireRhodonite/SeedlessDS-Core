#include "seedlessds/multiplayer.h"
#include "LAN.h"
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <string>
#include <thread>
#include <time.h>
#include <unistd.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

namespace {
constexpr uint64_t pause_grace_ms = 3000;
constexpr uint64_t interrupted_deadline = UINT64_MAX;

uint64_t elapsed_ms()
{
#ifdef __ANDROID__
    timespec now{};
    clock_gettime(CLOCK_BOOTTIME, &now);
    return uint64_t(now.tv_sec) * 1000 + uint64_t(now.tv_nsec) / 1000000;
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

std::string quote(const char* value)
{
    std::string result = "\"";
    for (const unsigned char* byte = reinterpret_cast<const unsigned char*>(value); *byte; byte++) {
        if (*byte == '"' || *byte == '\\') { result += '\\'; result += *byte; }
        else if (*byte < 32) {
            static const char hex[] = "0123456789abcdef";
            result += "\\u00"; result += hex[*byte >> 4]; result += hex[*byte & 15];
        } else result += *byte;
    }
    result += '"';
    return result;
}

std::string address(uint32_t raw)
{
    in_addr ip{raw};
    char output[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &ip, output, sizeof(output));
    return output;
}

struct SocketWake {
    int descriptors[2] = {-1, -1};
    bool pending = false;

    SocketWake() = default;
    SocketWake(const SocketWake&) = delete;
    SocketWake& operator=(const SocketWake&) = delete;
    ~SocketWake() { close(); }

    void close()
    {
        for (int& descriptor : descriptors) {
            if (descriptor >= 0) ::close(descriptor);
            descriptor = -1;
        }
        pending = false;
    }

    bool open(ENetSocketFailure* failure)
    {
        if (descriptors[0] >= 0) return true;
        if (pipe(descriptors) != 0) {
            enet_socket_failure_record(failure, "worker_pipe", errno);
            return false;
        }
        for (const int descriptor : descriptors) {
            if (fcntl(descriptor, F_SETFL, O_NONBLOCK) < 0) {
                enet_socket_failure_record(failure, "worker_pipe_nonblock", errno);
                close();
                return false;
            }
            if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
                enet_socket_failure_record(failure, "worker_pipe_cloexec", errno);
                close();
                return false;
            }
        }
        return true;
    }

    void signal()
    {
        if (pending || descriptors[1] < 0) return;
        const uint8_t value = 1;
        ssize_t result;
        do { result = write(descriptors[1], &value, sizeof(value)); }
        while (result < 0 && errno == EINTR);
        pending = result > 0 || (result < 0 && errno == EAGAIN);
    }

    void consume()
    {
        uint8_t buffer[64];
        ssize_t result;
        do { result = read(descriptors[0], buffer, sizeof(buffer)); }
        while (result > 0 || (result < 0 && errno == EINTR));
        pending = false;
    }
};

struct Session {
    std::mutex mutex;
    std::mutex lifecycle;
    std::condition_variable wake;
    SocketWake socket_wake;
    std::shared_ptr<melonDS::LAN> lan;
    std::thread worker;
    std::atomic<bool> enabled{false};
    std::atomic<bool> readable{false};
    std::atomic<bool> game{false};
    std::atomic<uint64_t> pause_deadline{0};
    bool stopping = false;
    bool receiving = false;
    void* wm_owner = nullptr;
    int mode = 0;
    uint8_t mac[6]{};
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t replies = 0;
    std::string error;
    std::string snapshot = "{\"mode\":0,\"game\":false,\"players\":[],\"rooms\":[],\"error\":\"\"}";

    ~Session() { stop(); }

    void update()
    {
        char address_mac[18];
        std::snprintf(address_mac, sizeof(address_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        std::ostringstream json;
        const auto wm = lan ? lan->GetWMServiceInfo() : melonDS::WMServiceInfo{};
        json << "{\"mode\":" << mode << ",\"game\":" << (game ? "true" : "false")
             << ",\"wm_session\":" << wm.Session << ",\"wm_peers\":" << wm.Peers
             << ",\"wm_version\":" << unsigned(melonDS::WMServiceVersion)
             << ",\"firmware_boot_mask\":" << (lan ? lan->GetFirmwareBootMask() : 0)
             << ",\"conservative_ack\":" << (lan && lan->UsesConservativeAck() ? "true" : "false")
             << ",\"interrupted\":" << (pause_deadline == interrupted_deadline ? "true" : "false")
             << ",\"mac\":" << quote(address_mac)
             << ",\"sent\":" << sent << ",\"received\":" << received
             << ",\"replies\":" << replies << ",\"error\":" << quote(error.c_str())
             << ",\"players\":[";
        bool separator = false;
        if (lan) for (const auto& player : lan->GetPlayerList()) {
            if (separator) json << ',';
            separator = true;
            json << "{\"id\":" << player.ID << ",\"name\":" << quote(player.Name)
                 << ",\"status\":" << player.Status << ",\"ip\":" << quote(address(player.Address).c_str())
                 << ",\"local\":" << (player.IsLocalPlayer ? "true" : "false")
                 << ",\"ping\":" << player.Ping << '}';
        }
        json << "],\"rooms\":[";
        separator = false;
        if (lan) for (const auto& room : lan->GetDiscoveryList()) {
            if (separator) json << ',';
            separator = true;
            json << "{\"ip\":" << quote(address(htonl(room.first)).c_str())
                 << ",\"name\":" << quote(room.second.SessionName)
                 << ",\"players\":" << unsigned(room.second.NumPlayers)
                 << ",\"maximum\":" << unsigned(room.second.MaxPlayers) << '}';
        }
        json << "]}";
        snapshot = json.str();
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            enabled = false;
            readable = false;
            pause_deadline = 0;
            stopping = true;
            wake.notify_all();
            socket_wake.signal();
        }
        if (worker.joinable()) worker.join();
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [&] { return !receiving; });
        lan.reset();
        socket_wake.close();
        mode = 0;
        error.clear();
        update();
    }

    void run()
    {
        std::unique_lock<std::mutex> lock(mutex);
        auto next_update = std::chrono::steady_clock::now();
        while (!stopping) {
            if (receiving) {
                wake.wait(lock, [&] { return stopping || !receiving; });
                continue;
            }
            socket_wake.consume();
            auto deadline = pause_deadline.load();
            if (deadline && game && enabled
                && (deadline == interrupted_deadline || elapsed_ms() >= deadline)
                && pause_deadline.compare_exchange_strong(deadline, interrupted_deadline)) {
                lan.reset();
                readable = false;
                wake.notify_all();
                mode = 0;
                error = "Disconnected after pausing the game.";
                update();
                break;
            }
            auto transport = lan;
            transport->Flush();
            const auto queued = transport->GetReceiveQueueSize();
            transport->Process();
            readable.store(transport->HasPendingPacket(), std::memory_order_release);
            transport->Flush();
            const bool received_packet = transport->GetReceiveQueueSize() > queued;
            if (received_packet) wake.notify_all();
            const auto now = std::chrono::steady_clock::now();
            if (now >= next_update) {
                update();
                next_update = now + std::chrono::milliseconds(100);
            }
            const auto socket = transport->GetSocket();
            pollfd descriptors[2] = {{socket, POLLIN, 0}, {socket_wake.descriptors[0], POLLIN, 0}};
            const bool process_again = received_packet || transport->HasPendingWMDispatch();
            lock.unlock();
            if (process_again) {
                std::this_thread::yield();
            } else {
                poll(descriptors, 2, 5);
            }
            lock.lock();
        }
    }
};

Session& session()
{
    static Session value;
    return value;
}

struct ReceiveContext {
    Session& value;
    std::unique_lock<std::mutex>& lock;
    std::shared_ptr<melonDS::LAN> transport;

    bool valid() const
    {
        return value.enabled && value.lan == transport;
    }

    static void wait(void* context, int timeout)
    {
        auto& call = *static_cast<ReceiveContext*>(context);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        call.value.receiving = true;
        while (call.valid()) {
            call.value.socket_wake.consume();
            call.transport->Flush();
            call.transport->Process();
            call.transport->Flush();
            call.value.readable.store(call.transport->HasPendingPacket(), std::memory_order_release);
            if (call.transport->GetReceiveQueueSize() != 0) break;
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) break;
            pollfd descriptors[2] = {{call.transport->GetSocket(), POLLIN, 0},
                                     {call.value.socket_wake.descriptors[0], POLLIN, 0}};
            call.lock.unlock();
            const int result = poll(descriptors, 2, static_cast<int>(remaining));
            const int error = errno;
            call.lock.lock();
            if (result < 0 && error != EINTR) break;
        }
        call.value.receiving = false;
        call.value.wake.notify_all();
    }
};

void begin(void* context)
{
    auto& value = *static_cast<Session*>(context);
    std::lock_guard<std::mutex> lock(value.mutex);
    if (value.lan && value.enabled) {
        value.lan->Begin(0);
        value.readable = false;
        value.socket_wake.signal();
    }
}
void end(void* context)
{
    auto& value = *static_cast<Session*>(context);
    std::lock_guard<std::mutex> lock(value.mutex);
    if (value.lan && value.enabled) {
        value.lan->End(0);
        value.readable = false;
        value.socket_wake.signal();
    }
}
int send(void* context, unsigned type, const uint8_t* data, int length, uint64_t timestamp, uint16_t aid)
{
    auto& value = *static_cast<Session*>(context);
    std::lock_guard<std::mutex> lock(value.mutex);
    if (!value.lan || !value.enabled || length < 0 || length > 2048 || (length && !data)) return 0;
    auto* packet = const_cast<uint8_t*>(data);
    int result = 0;
    switch (type) {
    case 0: result = value.lan->SendPacket(0, packet, length, timestamp); break;
    case 1: result = value.lan->SendCmd(0, packet, length, timestamp); break;
    case 2: result = value.lan->SendReply(0, packet, length, timestamp, aid); break;
    case 3: result = value.lan->SendAck(0, packet, length, timestamp); break;
    case 4: result = value.lan->SendProgress(0, timestamp); break;
    default: return 0;
    }
    if (result > 0) value.sent++;
    return result;
}
int receive(void* context, int host, uint8_t* data, uint64_t* timestamp)
{
    auto& value = *static_cast<Session*>(context);
    if (!host && !value.readable.load(std::memory_order_acquire)) return 0;
    std::unique_lock<std::mutex> lock(value.mutex);
    if (!value.lan || !value.enabled) return host ? -1 : 0;
    ReceiveContext call{value, lock, value.lan};
    const melonDS::LAN::ReceiveWait wait{&call, ReceiveContext::wait};
    const int result = host ? call.transport->RecvHostPacket(0, data, timestamp, &wait)
                            : call.transport->RecvPacket(0, data, timestamp, &wait);
    if (!call.valid()) return host ? -1 : 0;
    value.readable.store(call.transport->HasPendingPacket(), std::memory_order_release);
    if (result > 0 || result == -2) value.received++;
    return result;
}
uint16_t receive_replies(void* context, uint8_t* data, uint64_t timestamp, uint16_t aid_mask)
{
    auto& value = *static_cast<Session*>(context);
    std::unique_lock<std::mutex> lock(value.mutex);
    if (!value.lan || !value.enabled) return 0;
    ReceiveContext call{value, lock, value.lan};
    const melonDS::LAN::ReceiveWait wait{&call, ReceiveContext::wait};
    const auto result = call.transport->RecvReplies(0, data, timestamp, aid_mask, &wait);
    if (!call.valid()) return 0;
    value.readable.store(call.transport->HasPendingPacket(), std::memory_order_release);
    value.replies += __builtin_popcount(result);
    return result;
}
int conservative_ack(void* context)
{
    auto& value = *static_cast<Session*>(context);
    std::lock_guard<std::mutex> lock(value.mutex);
    return !value.lan || value.lan->UsesConservativeAck();
}
}

static void start_diagnostic(char* output, size_t capacity, const ENetSocketFailure& failure)
{
    if (!output || !capacity || !failure.operation) return;
    if (failure.retryOperation)
        std::snprintf(output, capacity, "operation=%s code=%d retry_operation=%s retry_code=%d",
                      failure.operation, failure.code, failure.retryOperation, failure.retryCode);
    else
        std::snprintf(output, capacity, "operation=%s code=%d", failure.operation, failure.code);
}

static int start_session(int mode, const char* name, const char* host, const uint8_t mac[6],
                         int maximum, const melonDS::LAN::GroupInterface* group,
                         char* diagnostic = nullptr, size_t capacity = 0)
{
    if (mode < SEEDLESS_MP_HOST || mode > SEEDLESS_MP_DISCOVER || !name || !host || !mac || maximum < 2 || maximum > 16
        || std::strlen(name) > 31 || std::strlen(host) > 253 || (mac[0] & 1)) {
        start_diagnostic(diagnostic, capacity, {"native_validate_arguments", -1, nullptr, 0});
        return -1;
    }
    auto& value = session();
    std::lock_guard<std::mutex> lifecycle(value.lifecycle);
    if (value.game) {
        start_diagnostic(diagnostic, capacity, {"native_check_game", -2, nullptr, 0});
        return -2;
    }
    value.stop();
    std::lock_guard<std::mutex> lock(value.mutex);
#ifdef __ANDROID__
    char policy[PROP_VALUE_MAX]{};
    if (mode != SEEDLESS_MP_DISCOVER && __system_property_get("debug.seedlessds.wm_hle", policy)
        && std::strcmp(policy, "0") == 0) {
        start_diagnostic(diagnostic, capacity, {"native_check_wireless_policy", -3, nullptr, 0});
        value.error = "Wireless support is disabled for this multiplayer room.";
        value.update();
        return -3;
    }
#endif
    value.lan = std::make_shared<melonDS::LAN>();
    ENetSocketFailure failure{};
    bool success = mode == SEEDLESS_MP_HOST ? value.lan->StartHost(name, maximum, true, group, diagnostic ? &failure : nullptr)
        : mode == SEEDLESS_MP_JOIN ? value.lan->StartClient(name, host, true, group, diagnostic ? &failure : nullptr) : value.lan->StartDiscovery();
    if (!success) {
        start_diagnostic(diagnostic, capacity, failure);
        value.lan.reset();
        value.error = "Could not open the multiplayer session. Check the address and local network.";
        value.update();
        return -3;
    }
    failure = {};
    if (!value.socket_wake.open(diagnostic ? &failure : nullptr)) {
        start_diagnostic(diagnostic, capacity, failure);
        value.lan.reset();
        value.error = "Could not open the multiplayer worker signal.";
        value.update();
        return -3;
    }
    value.lan->SetFlushOnSend(true);
    std::memcpy(value.mac, mac, sizeof(value.mac));
    value.sent = value.received = value.replies = 0;
    value.mode = mode;
    value.stopping = false;
    value.enabled = mode != SEEDLESS_MP_DISCOVER;
    value.update();
    value.worker = std::thread([&value] { value.run(); });
    return 0;
}

extern "C" int seedless_mp_start(int mode, const char* name, const char* host, const uint8_t mac[6])
{
    return seedless_mp_start_maximum(mode, name, host, mac, 16);
}

extern "C" int seedless_mp_start_maximum(int mode, const char* name, const char* host, const uint8_t mac[6], int maximum)
{
#ifdef __ANDROID__
    return -1;
#else
    return start_session(mode, name, host, mac, maximum, nullptr);
#endif
}

extern "C" int seedless_mp_start_direct(int mode, const char* name, uint32_t local, uint32_t owner,
                                       unsigned interface_index, const uint8_t mac[6], int maximum)
{
    return seedless_mp_start_direct_report(mode, name, local, owner, interface_index, mac, maximum, nullptr, 0);
}

extern "C" int seedless_mp_start_direct_report(int mode, const char* name, uint32_t local, uint32_t owner,
                                              unsigned interface_index, const uint8_t mac[6], int maximum,
                                              char* diagnostic, size_t capacity)
{
    if (diagnostic && capacity) diagnostic[0] = '\0';
    if ((mode != SEEDLESS_MP_HOST && mode != SEEDLESS_MP_JOIN) || !local || !owner || !interface_index
        || (mode == SEEDLESS_MP_HOST) != (local == owner)) {
        start_diagnostic(diagnostic, capacity, {"native_validate_route", -1, nullptr, 0});
        return -1;
    }
    const melonDS::LAN::GroupInterface group{local, owner, interface_index};
    return start_session(mode, name, "", mac, maximum, &group, diagnostic, capacity);
}

extern "C" int seedless_mp_bind_interface(int socket_fd, unsigned interface_index)
{
    return enet_socket_bind_interface(socket_fd, interface_index);
}

extern "C" int seedless_mp_stop()
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.lifecycle);
    if (value.game && value.enabled) return -2;
    value.stop();
    return 0;
}
extern "C" int seedless_mp_pause(int paused)
{
    auto& value = session();
    if (!value.enabled) return 0;
    auto deadline = value.pause_deadline.load();
    if (paused) {
        if (!deadline)
            value.pause_deadline.compare_exchange_strong(deadline, elapsed_ms() + pause_grace_ms);
    } else {
        while (deadline && deadline != interrupted_deadline) {
            auto next = elapsed_ms() >= deadline ? interrupted_deadline : uint64_t{0};
            if (value.pause_deadline.compare_exchange_weak(deadline, next)) break;
        }
    }
    return value.pause_deadline == interrupted_deadline;
}
extern "C" size_t seedless_mp_snapshot(char* output, size_t capacity)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    size_t size = value.snapshot.size();
    if (output && capacity > size) std::memcpy(output, value.snapshot.c_str(), size + 1);
    return size;
}
extern "C" int seedless_mp_enabled() { return session().enabled; }
extern "C" int seedless_mp_game_started(int firmware_boot)
{
    auto& value = session();
    std::lock_guard<std::mutex> lifecycle(value.lifecycle);
    std::lock_guard<std::mutex> lock(value.mutex);
    if (value.lan && value.enabled) value.lan->SetFirmwareBoot(firmware_boot != 0);
    value.socket_wake.signal();
    value.game = true;
    return value.enabled;
}
extern "C" void seedless_mp_game_stopped()
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    value.game = false;
    value.wake.notify_all();
    if (value.lan) value.lan->SetFirmwareBoot(false);
    value.socket_wake.signal();
    value.pause_deadline = 0;
    if (!value.lan) value.enabled = false;
    value.update();
}
extern "C" void seedless_mp_mac(uint8_t mac[6])
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    std::memcpy(mac, value.mac, sizeof(value.mac));
}
extern "C" void seedless_mp_bind(seedless_wifi_callbacks_t* callbacks)
{
    callbacks->transport = &session();
    callbacks->begin = begin;
    callbacks->end = end;
    callbacks->send = send;
    callbacks->receive = receive;
    callbacks->replies = receive_replies;
    callbacks->conservative_ack = conservative_ack;
}

extern "C" void seedless_mp_bind_wm(void* owner)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    value.wm_owner = owner;
}

namespace melonDS::Platform {
static bool owns(const Session& value, const void* owner)
{
    return owner && value.wm_owner == owner && value.enabled && value.game && value.lan;
}

WMServiceInfo MP_WMInfo(void* owner)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    return owns(value, owner) ? value.lan->GetWMServiceInfo() : WMServiceInfo{};
}

void MP_WMAbort(void* owner)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    if (!owns(value, owner)) return;
    value.lan->EndSession();
    value.enabled = false;
    value.readable = false;
    value.stopping = true;
    value.error = "This wireless mode is not supported in this multiplayer room.";
    value.wake.notify_all();
    value.socket_wake.signal();
    value.update();
}

u16 MP_WMSend(u16 peers, const u8* data, u16 length, void* owner)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    if (!owns(value, owner)) return 0;
    const u16 sent = value.lan->SendWMService(peers, data, length);
    if (sent) { ++value.sent; value.socket_wake.signal(); }
    return sent;
}

int MP_WMReceive(WMServicePacket& packet, void* owner)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    if (!owns(value, owner)) return -1;
    const int received = value.lan->ReceiveWMService(packet);
    if (value.lan->HasPendingWMDispatch()) value.socket_wake.signal();
    if (received > 0) ++value.received;
    return received;
}

void MP_WMReset(void* owner)
{
    auto& value = session();
    std::lock_guard<std::mutex> lock(value.mutex);
    if (owns(value, owner)) value.lan->ResetWMService();
}

void Sleep(u64 microseconds)
{
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}
}
