#ifndef SEEDLESSDS_WIFI_PLATFORM_H
#define SEEDLESSDS_WIFI_PLATFORM_H

#include "types.h"

namespace melonDS { struct WMServiceInfo; struct WMServicePacket; }

namespace melonDS::Platform
{
enum class LogLevel { Debug, Info, Warn, Error };
void Log(LogLevel level, const char* format, ...);
struct Mutex;
Mutex* Mutex_Create();
void Mutex_Free(Mutex* mutex);
void Mutex_Lock(Mutex* mutex);
void Mutex_Unlock(Mutex* mutex);
u64 GetMSCount();
void Sleep(u64 microseconds);
WMServiceInfo MP_WMInfo(void* owner);
void MP_WMAbort(void* owner);
u16 MP_WMSend(u16 peers, const u8* data, u16 length, void* owner);
int MP_WMReceive(WMServicePacket& packet, void* owner);
void MP_WMReset(void* owner);
void MP_Begin(void* context);
void MP_End(void* context);
int MP_SendPacket(u8* data, int length, u64 timestamp, void* context);
int MP_SendCmd(u8* data, int length, u64 timestamp, void* context);
int MP_SendReply(u8* data, int length, u64 timestamp, u16 aid, void* context);
int MP_SendAck(u8* data, int length, u64 timestamp, void* context);
int MP_SendProgress(u64 timestamp, void* context);
bool MP_ConservativeAck(void* context);
int MP_RecvPacket(u8* data, u64* timestamp, void* context);
int MP_RecvHostPacket(u8* data, u64* timestamp, void* context);
u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aid_mask, void* context);
}
#endif
