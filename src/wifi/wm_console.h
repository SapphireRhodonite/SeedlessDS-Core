#ifndef SEEDLESS_WM_CONSOLE_H
#define SEEDLESS_WM_CONSOLE_H
#include "wm_host.h"
#include "WMServiceTrace.h"
#include <array>
#include <algorithm>
namespace melonDS {
class WMConsole {
public:
    explicit WMConsole(const seedless_wm_host& host) : Host(host), UserData(host.owner), MainRAM(host.ram), MainRAMMask(host.ram_mask) {}
    seedless_wm_host Host;
    void* const UserData;
    u8* const MainRAM;
    const u32 MainRAMMask;
    u64 Timestamp() const { return *Host.cycles / 2; }
    u16 VCount() const { return *Host.vcount; }
    u16 Control(unsigned cpu) const { return *Host.control[cpu]; }
    unsigned Level(unsigned cpu) const { return Host.level(UserData, cpu); }
    u32 Peek() const { return Host.peek(UserData); }
    void Pop() { Host.pop(UserData); }
    void Reply(u32 word) { Host.reply(UserData, word); }
    void Interrupt(unsigned cpu, unsigned irq) { Host.interrupt(UserData, cpu, irq); }
    void ARM7Write8(u32 a, u8 v) { Host.write8(UserData, a, v); }
    void ARM7Write16(u32 a, u16 v) { Host.write16(UserData, a, v); }
    void ARM7Write32(u32 a, u32 v) { Host.write32(UserData, a, v); }
    std::array<u8,6> Mac() const { std::array<u8,6> mac; std::copy_n(Host.firmware+0x36, 6, mac.begin()); return mac; }
    u16 Channels() const { return Host.firmware[0x3c] | (u16(Host.firmware[0x3d]) << 8); }
    void Stop() { Host.stop(UserData); }
    bool IsWMTraceActive() const { return Trace.Active; }
    u64 TraceWMServiceEvent(const WMServiceTraceMetadata&, const u8*, u32, const u8*, u32);
    void TraceBoundary();
    WMServiceTrace Trace;
};
}
#endif
