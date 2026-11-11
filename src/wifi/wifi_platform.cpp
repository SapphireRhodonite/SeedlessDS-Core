#include "Platform.h"
#include "Wifi.h"
#include "seedlessds/wifi.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <new>
#ifdef __ANDROID__
#include <android/log.h>
#endif

struct seedless_wifi {
    melonDS::Wifi controller;
    explicit seedless_wifi(const seedless_wifi_callbacks_t& host) : controller(host) {}
};

extern "C" seedless_wifi_t* seedless_wifi_create(const seedless_wifi_callbacks_t* host)
{
    if (!host || !host->firmware || !host->schedule || !host->cancel || !host->irq)
        return nullptr;
    auto* wifi = new (std::nothrow) seedless_wifi(*host);
    if (wifi) wifi->controller.Reset();
    return wifi;
}

extern "C" void seedless_wifi_destroy(seedless_wifi_t* wifi) { delete wifi; }
extern "C" void seedless_wifi_reset(seedless_wifi_t* wifi) { wifi->controller.Reset(); }
extern "C" void seedless_wifi_power(seedless_wifi_t* wifi, uint32_t power) { wifi->controller.SetPowerCnt(power); }
extern "C" void seedless_wifi_tick(seedless_wifi_t* wifi) { wifi->controller.USTimer(0); }
extern "C" void seedless_wifi_flush(seedless_wifi_t* wifi) { wifi->controller.FlushMPAck(); }
extern "C" void seedless_wifi_sync(seedless_wifi_t* wifi) { wifi->controller.SyncTransport(); }
extern "C" int seedless_wifi_is_client(const seedless_wifi_t* wifi) { return wifi->controller.IsClient(); }
extern "C" uint16_t seedless_wifi_read(seedless_wifi_t* wifi, uint32_t address) { return wifi->controller.Read(address); }
extern "C" void seedless_wifi_write(seedless_wifi_t* wifi, uint32_t address, uint16_t value) { wifi->controller.Write(address, value); }

namespace melonDS::Platform
{
struct Mutex { std::mutex value; };
Mutex* Mutex_Create() { return new Mutex; }
void Mutex_Free(Mutex* mutex) { delete mutex; }
void Mutex_Lock(Mutex* mutex) { mutex->value.lock(); }
void Mutex_Unlock(Mutex* mutex) { mutex->value.unlock(); }
u64 GetMSCount()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void Log(LogLevel level, const char* format, ...)
{
    if (level == LogLevel::Debug) return;
    va_list arguments;
    va_start(arguments, format);
#ifdef __ANDROID__
    int priority = level == LogLevel::Error ? ANDROID_LOG_ERROR
        : level == LogLevel::Warn ? ANDROID_LOG_WARN : ANDROID_LOG_INFO;
    __android_log_vprint(priority, "SeedlessWifi", format, arguments);
#else
    std::vfprintf(stderr, format, arguments);
#endif
    va_end(arguments);
}
void MP_Begin(void* context)
{
    auto* host = static_cast<seedless_wifi_callbacks_t*>(context);
    if (host->begin) host->begin(host->transport);
}
void MP_End(void* context)
{
    auto* host = static_cast<seedless_wifi_callbacks_t*>(context);
    if (host->end) host->end(host->transport);
}
static int Send(unsigned type, u8* data, int length, u64 timestamp, u16 aid, void* context)
{
    auto* host = static_cast<seedless_wifi_callbacks_t*>(context);
    return host->send ? host->send(host->transport, type, data, length, timestamp, aid) : 0;
}
int MP_SendPacket(u8* data, int length, u64 timestamp, void* context) { return Send(0, data, length, timestamp, 0, context); }
int MP_SendCmd(u8* data, int length, u64 timestamp, void* context) { return Send(1, data, length, timestamp, 0, context); }
int MP_SendReply(u8* data, int length, u64 timestamp, u16 aid, void* context) { return Send(2, data, length, timestamp, aid, context); }
int MP_SendAck(u8* data, int length, u64 timestamp, void* context) { return Send(3, data, length, timestamp, 0, context); }
int MP_SendProgress(u64 timestamp, void* context) { return Send(4, nullptr, 0, timestamp, 0, context); }
bool MP_ConservativeAck(void* context)
{
    auto* host = static_cast<seedless_wifi_callbacks_t*>(context);
    return !host->conservative_ack || host->conservative_ack(host->transport);
}
static int Receive(int host_packet, u8* data, u64* timestamp, void* context)
{
    auto* host = static_cast<seedless_wifi_callbacks_t*>(context);
    return host->receive ? host->receive(host->transport, host_packet, data, timestamp) : 0;
}
int MP_RecvPacket(u8* data, u64* timestamp, void* context) { return Receive(0, data, timestamp, context); }
int MP_RecvHostPacket(u8* data, u64* timestamp, void* context) { return Receive(1, data, timestamp, context); }
u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aid_mask, void* context)
{
    auto* host = static_cast<seedless_wifi_callbacks_t*>(context);
    return host->replies ? host->replies(host->transport, data, timestamp, aid_mask) : 0;
}
}
