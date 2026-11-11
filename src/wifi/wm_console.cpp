#include "wm_console.h"
#include "WMService.h"
#include "Platform.h"
#include "seedlessds/platform.h"

static FILE* openTrace(const char* path, const char* mode)
{
    return static_cast<FILE*>(platform_file_open(path, mode));
}
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
namespace melonDS {
u64 WMConsole::TraceWMServiceEvent(const WMServiceTraceMetadata& metadata, const u8* data, u32 dataLength, const u8* extra, u32 extraLength)
{
    auto& trace = Trace;
    auto* entry = trace.Reserve();
    if (!entry) return ~u64{0};
    auto& event = *entry;
    event.HostNanoseconds = WMServiceTrace::Now();
    event.SystemTimestamp = Timestamp();
    event.Frame = *Host.frame;
    event.Kind = WMServiceTrace::Service;
    event.FIFOCnt9 = Control(0); event.FIFOCnt7 = Control(1);
    event.FIFOLevel9 = Level(1); event.FIFOLevel7 = Level(0);
    WMServiceTrace::Metadata(event, metadata, dataLength, extraLength);
    if (metadata.RecordFlags & WMServiceTrace::InvalidData) trace.Flag(event, WMServiceTrace::InvalidData);
    if (metadata.RecordFlags & WMServiceTrace::TruncatedData) trace.Flag(event, WMServiceTrace::TruncatedData);
    const u64 total = u64(dataLength) + extraLength;
    event.DataLength = static_cast<u32>(std::min<u64>(total, ~u32{0}));
    if (total > ~u32{0}) trace.Flag(event, WMServiceTrace::InvalidData);
    if (total > sizeof(event.Data)) trace.Flag(event, WMServiceTrace::TruncatedData);
    const u32 first = std::min(dataLength, static_cast<u32>(sizeof(event.Data)));
    if (first && !data)
    {
        trace.Flag(event, WMServiceTrace::InvalidData);
        return event.Sequence;
    }
    if (first) std::memcpy(event.Data, data, first);
    event.DataCopied = first;
    const u32 second = std::min(extraLength, static_cast<u32>(sizeof(event.Data)) - first);
    if (second && !extra) trace.Flag(event, WMServiceTrace::InvalidData);
    else if (second)
    {
        std::memcpy(event.Data + first, extra, second);
        event.DataCopied += second;
    }
    return event.Sequence;
}
void WMConsole::TraceBoundary()
{
#ifdef __ANDROID__
    char id[PROP_VALUE_MAX]{};
    __system_property_get("debug.seedlessds.wm_trace", id);
    if (!WMServiceTrace::ValidId(id)) id[0] = 0;
    if (Trace.LastRequest == id) return;
    if (Trace.Active && !Trace.Finish(*Host.frame, Timestamp(), 1, openTrace)) Platform::Log(Platform::LogLevel::Error, "WM trace write failed\n");
    Trace.LastRequest = id;
    if (*id && !Trace.Begin(Host.directory, id, reinterpret_cast<uintptr_t>(UserData), 0, Host.jit, *Host.frame, Timestamp()))
        Platform::Log(Platform::LogLevel::Error, "WM trace start failed\n");
#endif
}
}
struct seedless_wm {
    melonDS::WMConsole console;
    melonDS::WMService service;
    explicit seedless_wm(const seedless_wm_host& host) : console(host), service(console) {}
};
extern "C" seedless_wm* seedless_wm_create(const seedless_wm_host* host) { return new(std::nothrow) seedless_wm(*host); }
extern "C" void seedless_wm_destroy(seedless_wm* wm) {
    wm->service.Reset();
    if (wm->console.Trace.Active) wm->console.Trace.Finish(*wm->console.Host.frame, wm->console.Timestamp(), 2, openTrace);
    delete wm;
}
extern "C" void seedless_wm_poll(seedless_wm* wm) { wm->service.Poll(); }
extern "C" void seedless_wm_vblank(seedless_wm* wm, uint64_t edge) { wm->service.WaitAtVBlank(edge); }
extern "C" void seedless_wm_capture(seedless_wm* wm, uint32_t word) { wm->service.Capture(word); }
extern "C" void seedless_wm_clear(seedless_wm* wm) { wm->service.ClearFIFO(); }
extern "C" int seedless_wm_blocks(seedless_wm* wm) { return wm->service.BlocksARM7(); }
extern "C" int seedless_wm_host_pending(seedless_wm* wm) { return wm->service.CanPollHost(); }
extern "C" int seedless_wm_needs_guest_cycles(seedless_wm* wm) { return wm->service.NeedsGuestCycles(); }
extern "C" void seedless_wm_poll_host(seedless_wm* wm) { wm->service.PollHost(); }
extern "C" void seedless_wm_frame(seedless_wm* wm, uint32_t budget) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    wm->service.SetFrameDeadline(budget ? std::chrono::duration_cast<std::chrono::microseconds>(now).count() + budget : 0);
    wm->console.TraceBoundary();
}
