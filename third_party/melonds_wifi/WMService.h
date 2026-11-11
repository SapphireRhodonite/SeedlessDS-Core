#ifndef MELONDS_WMSERVICE_H
#define MELONDS_WMSERVICE_H

#include <memory>
#include "types.h"

namespace melonDS
{
class WMConsole;

class WMService
{
public:
    explicit WMService(WMConsole& nds);
    ~WMService();
    WMService(const WMService&) = delete;
    WMService& operator=(const WMService&) = delete;

    void Reset();
    void Poll();
    void PollHost();
    void WaitAtVBlank(u64 edge);
    bool CanPollHost() const;
    bool NeedsGuestCycles() const;
    void SetFrameDeadline(u64 deadline);
    void Capture(u32 word);
    void ClearFIFO();
    bool BlocksARM7() const;
    bool Active() const;

private:
    void StopIfTerminal();
    struct Impl;
    std::unique_ptr<Impl> State;
};
}

#endif
