#include "WMService.h"
#include "WMServiceTrace.h"
#include "wm_console.h"
#include "Platform.h"
#include "MPInterface.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstring>
#include <deque>
#include <optional>
#include <vector>

namespace melonDS
{
namespace
{
using Bytes = std::vector<u8>;
using MAC = std::array<u8, 6>;
using TraceMetadata = WMServiceTraceMetadata;
using TraceStage = TraceMetadata::Stage;
using TraceReason = TraceMetadata::Reason;

u16 U16(const u8* p) { return p[0] | (u16(p[1]) << 8); }
u32 U32(const u8* p) { return U16(p) | (u32(U16(p + 2)) << 16); }
u64 U64(const u8* p) { return U32(p) | (u64(U32(p + 4)) << 32); }
void Put16(u8* p, u16 v) { p[0] = v; p[1] = v >> 8; }
void Put32(u8* p, u32 v) { Put16(p, v); Put16(p + 2, v >> 16); }
void Put64(u8* p, u64 v) { Put32(p, v); Put32(p + 4, v >> 32); }
u64 Now()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr u32 Magic = 0x31534D57;
constexpr u32 CallbackFlag = 0x027FFF96;
constexpr size_t MaxCallbacks = 1024;
constexpr size_t MaxOperations = 32;
constexpr size_t MaxPendingBeacons = 256;
constexpr u64 TicksPerMillisecond = 33514;
constexpr u64 ClockFrequency = 33513982;
constexpr u16 LegacyChildMPLine = 240;
constexpr u64 HeartbeatIntervalUs = 25000;
std::atomic<u64> EpochCounter{0};

enum class Backend { Undecided, LLE, HLE };
enum class Message : u8
{
    Offer = 1, Ready, Reject, Advertisement, Connect, Accepted,
    Disconnect, MPState, PortData, PortAck, Heartbeat, RoundPoll, RoundReply, RoundAck, ReservedPrepared, ScanProbe, ScanResponse, RoundIntent, RoundCancel, BeaconTimingSupport
};

struct WMABI
{
    bool Modern = true;
    bool Firmware = false;
    bool SplitPower = false;
    u16 WM7Base = 0x200;
    u16 StatusBase = 0x500;
    u16 ReplyBase = 0xE00;
    u16 MP = 0xC;
    u16 RXSelector = 0x70;
    u16 RXSize = 0x72;
    u16 RX = 0x74;
    u16 Ready = 0x86;
    u16 Link = 0xBC;
    u16 PowerSave = 0xC6;
    u16 LocalMAC = 0xE0;
    u16 OperatingMode = 0xE6;
    u16 ParentParameters = 0xE8;
    u16 ChildMACs = 0x128;
    u16 Members = 0x182;
    u16 ScanBuffer = 0x184;
    u16 AID = 0x188;
    u16 ParentMAC = 0x18A;
    u16 Channel = 0x190;
    u16 EffectiveTX = 0x38;
    u16 EffectiveRX = 0x3A;
    u16 AllowedChannels = 0x1F4;
    u16 MPChannels = 0x1F6;
    u16 Lifetime = 0x7B8;
    u16 Frequency = 0x3C;
    u16 Retry = 0x9C;
    u16 Booleans = 0x96;
    u16 IgnoreFatal = 0xDE;
    u16 Indications = 0;
    u16 ConnectedTGID = 0;
    u16 LastSeen = 0;
    constexpr bool LegacyRetail() const { return !Modern && !Firmware; }
};

constexpr WMABI RetailABI{};
constexpr WMABI FirmwareABI = [] {
    WMABI abi;
    abi.Modern = false;
    abi.Firmware = true;
    abi.WM7Base = 0x600;
    abi.StatusBase = 0x700;
    abi.ReplyBase = 0x1200;
    abi.Ready = 0x8A;
    abi.MP = 0x10;
    abi.RXSelector = 0x46;
    abi.RXSize = 0x4A;
    abi.RX = 0x4C;
    abi.EffectiveTX = 0x192;
    abi.EffectiveRX = 0x194;
    return abi;
}();
constexpr WMABI LegacyRetailABI = [] {
    WMABI abi;
    abi.Modern = false;
    abi.RXSelector = 0x44;
    abi.RXSize = 0x46;
    abi.RX = 0x48;
    abi.Link = 0xB8;
    abi.PowerSave = 0xC2;
    abi.LocalMAC = 0xDC;
    abi.OperatingMode = 0xE2;
    abi.ParentParameters = 0xE4;
    abi.ChildMACs = 0x124;
    abi.Members = 0x17E;
    abi.ScanBuffer = 0x180;
    abi.AID = 0x184;
    abi.ParentMAC = 0x186;
    abi.Channel = 0x18C;
    abi.EffectiveTX = 0x18E;
    abi.EffectiveRX = 0x190;
    abi.AllowedChannels = 0x1F0;
    abi.MPChannels = 0x1F2;
    abi.Lifetime = 0x7B4;
    abi.Frequency = 0x34;
    abi.Retry = 0x98;
    abi.Booleans = 0x92;
    abi.IgnoreFatal = 0xDA;
    abi.Indications = 0xBE;
    abi.ConnectedTGID = 0xB6;
    abi.LastSeen = 0x734;
    return abi;
}();

constexpr WMABI ExpandedLegacyRetailABI = [] {
    WMABI abi = LegacyRetailABI;
    abi.SplitPower = true;
    abi.WM7Base = 0xA00;
    abi.StatusBase = 0xD00;
    abi.ReplyBase = 0x1600;
    abi.Lifetime = 0x7B0;
    abi.LastSeen = 0x730;
    return abi;
}();

const WMABI& ABIFor(u8 profile)
{
    return profile == 2 ? FirmwareABI : profile == 3 ? LegacyRetailABI
        : profile == 4 ? ExpandedLegacyRetailABI : RetailABI;
}

u16 MPChannels(u16 allowed)
{
    const u16 mask = (allowed >> 1) & 0x1FFF;
    if (!mask) return 0;
    unsigned first = 0, last = 12;
    while (!(mask & (1u << first))) ++first;
    while (!(mask & (1u << last))) --last;
    if (last - first < 5) return 1u << first;
    const int midpoint = (first + last) / 2;
    int center = midpoint;
    for (unsigned step = 0; step < last - first; step++)
    {
        const int distance = (step + 1) / 2;
        center = midpoint + (step & 1 ? distance : -distance);
        if (mask & (1u << center)) break;
    }
    u16 result = (1u << first) | (1u << last);
    if (center - static_cast<int>(first) >= 5 && static_cast<int>(last) - center >= 5)
        result |= 1u << center;
    return result;
}

struct Request
{
    u32 Word = 0;
    u32 Address = 0;
    std::array<u8, 256> Data{};
    Bytes Payload;
    Bytes Extra;
    bool Valid = true;
    u16 API() const { return U16(Data.data()); }
    u16 Half(u32 offset) const { return U16(Data.data() + offset); }
    u32 Arg(u32 offset) const { return U32(Data.data() + offset); }
};

struct Callback
{
    std::array<u8, 256> Data{};
    Bytes Receive;
    u32 ReceiveBase = 0;
    bool End = false;
    u64 RoundID = 0;
    u64 RoundEpoch = 0;
    u64 RoundOwner = 0;
    u8 RoundEvent = 0;
    bool AckMetadata = false;
    u64 ScanToken = 0;
    u64 ScanEpoch = 0;
    u32 ScanOwner = 0;
};

struct Peer
{
    u64 Epoch = 0;
    u64 Seen = 0;
    u64 SeenEmulated = 0;
    u64 NextHeartbeat = 0;
    u64 Connection = 0;
    bool Ready = false;
    bool TimingSupportSent = false;
    bool TimingSupportReceived = false;
    bool Connected = false;
    bool MP = false;
    u64 MPGeneration = 0;
    bool MPClosed = false;
    u8 Profile = 0;
    u16 AID = 0;
    MAC Mac{};
    Bytes BSS;
    std::array<u16, 8> ReceivedSequences{0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
};

struct BeaconClock
{
    u64 BaseUS = 0;
    u64 BaseTick = 0;
    bool Running = false;

    u64 ElapsedUS(u64 tick) const
    {
        if (!Running || tick < BaseTick) return 0;
        const u64 delta = tick - BaseTick;
        return (delta / ClockFrequency) * 1000000 + (delta % ClockFrequency) * 1000000 / ClockFrequency;
    }

    u64 At(u64 tick) const { return BaseUS + ElapsedUS(tick); }

    void Start(u64 tick)
    {
        if (!Running) { BaseTick = tick; Running = true; }
    }

    void Stop(u64 tick)
    {
        BaseUS = At(tick);
        Running = false;
    }
};

struct BeaconTiming
{
    u64 EndUS;
    u64 OriginUS;
    u16 VTSF;
};

bool RadioReached(u64 now, u64 target) { return now - target < (u64{1} << 63); }

struct ScanObservation
{
    u64 Epoch = 0;
    u16 Offset = 0;
    u16 Size = 0;
    u8 PeerID = 0;
};

struct Operation
{
    u64 ID = 0;
    u64 Started = 0;
    u16 Destinations = 0;
    u16 RequestedDestinations = 0;
    u16 Pending = 0;
    u16 Queued = 0;
    u16 Sent = 0;
    u16 Failed = 0;
    u16 Port = 0;
    u16 Priority = 0;
    u16 Size = 0;
    u16 Sequence = 0xFFFF;
    u16 Retries = 0;
    bool Prepared = false;
    bool WaitedAtVBlank = false;
    u32 DataAddress = 0;
    u32 Function = 0;
    u32 Argument = 0;
    std::array<u64, 16> Connections{};
    Bytes Data;
};

struct PreparedPacket
{
    u64 ID = 0;
    u64 WaitUntil = 0;
    u16 Header = 0;
    u16 Destinations = 0;
    Bytes Data;
    std::vector<u64> Operations;
};

struct RoundResponse
{
    u64 Epoch = 0;
    u64 Connection = 0;
    u64 Preparation = 0;
    u64 MPGeneration = 0;
    u16 AID = 0;
    u16 Header = 0;
    bool Received = false;
    bool Prepared = false;
    bool IntentSent = false;
    bool FallbackSent = false;
    bool CancelSent = false;
    Bytes Data;
};

struct MPRound
{
    u64 ID = 0;
    u64 Started = 0;
    u64 WaitUntil = 0;
    u16 PollBitmap = 0;
    u16 ReplySlotTime = 0;
    u16 Expected = 0;
    u16 Remaining = 0;
    u16 Failed = 0;
    bool Closing = false;
    bool Terminating = false;
    bool Polled = false;
    bool VSync = false;
    PreparedPacket Packet;
    std::array<RoundResponse, 16> Responses{};
};

struct RoundIntent
{
    enum class Stage { Pending, Promoted, Retired };
    Stage State = Stage::Pending;
    unsigned PeerID = 0;
    u64 LocalEpoch = 0;
    u64 RemoteEpoch = 0;
    Bytes Data;
};

struct PortPayload
{
    u16 Port = 0;
    u16 Sequence = 0xFFFF;
    u16 Destinations = 0xFFFF;
    u16 Length = 0;
    u32 Offset = 0;
};

bool ParsePacket(u16 header, const u8* data, size_t size, std::vector<PortPayload>& ports)
{
    if (size > 516 || (size & 1)) return false;
    size_t offset = 0;
    while (offset < size)
    {
        if (!ports.empty())
        {
            if (size - offset < 2) return false;
            header = U16(data + offset);
            offset += 2;
        }
        PortPayload port;
        port.Port = (header >> 8) & 15;
        port.Length = (header & 255) ? (header & 255) * 2 : 512;
        port.Offset = offset;
        const size_t length = port.Length + ((header & 0x800) ? 2 : 0) + ((header & 0x1000) ? 2 : 0);
        if (length > size - offset || ports.size() >= MaxOperations) return false;
        offset += port.Length;
        if (header & 0x800) { port.Sequence = U16(data + offset) << 1; offset += 2; }
        if (header & 0x1000) { port.Destinations = U16(data + offset); offset += 2; }
        ports.push_back(port);
    }
    return true;
}
}

struct WMService::Impl
{
    WMConsole& Console;
    Backend Mode = Backend::Undecided;
    bool EverActive = false;
    bool Initialized = false;
    bool InFlight = false;
    bool Closing = false;
    bool Failed = false;
    bool FailureReported = false;
    bool Polling = false;
    bool Capable = false;
    bool Parent = false;
    bool MP = false;
    std::optional<bool> BeaconIndication;
    bool ScanPending = false;
    bool ScanExConfigured = false;
    bool ConnectPending = false;
    bool Terminal = false;
    bool StopRequested = false;
    u64 Session = 0;
    int LocalID = -1;
    int ParentID = -1;
    u16 Present = 0;
    u16 PresenceRecipients = 0;
    u16 OwnAID = 0;
    u16 Members = 0;
    u16 ReadyBitmap = 0;
    u16 Channel = 0;
    u16 AllowedChannels = 0;
    u16 ParentSize = 0;
    u16 ChildSize = 0;
    u16 MaxParentSize = 0;
    u16 MaxChildSize = 0;
    u16 BeaconPeriod = 100;
    u16 MaxEntry = 15;
    u16 RXSize = 0;
    u16 RXSelector = 0;
    u16 LifeTime = 0xFFFF;
    u16 FrameLifeTime = 5;
    u8 Profile = 0;
    u32 WM9 = 0;
    u32 WM7 = 0;
    u32 Status = 0;
    u32 Reply = 0;
    u32 RX = 0;
    u32 TX = 0;
    u16 TXSize = 0;
    u64 Epoch = 0;
    u64 NextOperation = 1;
    u64 NextConnection = 1;
    u64 NextNetworkTick = 0;
    u64 NextOffer = 0;
    u64 NextBeacon = 0;
    u64 ScanDeadline = 0;
    u64 ScanToken = 0;
    u64 NextScanToken = 1;
    u64 ScanEpoch = 0;
    u32 ScanOwner = 0;
    u16 ScanMask = 0;
    u16 ScanChannel = 0;
    u16 ScanInterval = 0;
    u16 ScanElapsed = 0;
    u16 ScanBudget = 0;
    u16 ScanBytes = 0;
    u16 ScanCount = 0;
    u16 ScanSSIDPrefix = 32;
    std::array<ScanObservation, 16> ScanObservations{};
    Bytes ScanData;
    u64 ConnectDeadline = 0;
    u64 ConnectionID = 0;
    u64 FrameDeadline = 0;
    u64 NextSendTick = 0;
    u64 NextPrepareTick = 0;
    u64 NextRound = 1;
    u64 MPGeneration = 0;
    u64 NextMPGeneration = 1;
    u64 NextPreparation = 1;
    u64 LastParentRound = 0;
    u16 LastVCount = 0;
    BeaconClock RadioClock;
    u64 BeaconScheduleOrigin = 0;
    std::optional<u64> ConfirmedReplyTick;
    std::deque<BeaconTiming> PendingBeacons;
    u16 BeaconVTSF = 0;
    bool BeaconPhaseValid = false;
    u16 RepeatedVCount = 0;
    u64 RepeatedVCountFrame = 0;
    bool ChildAdjustPending = false;
    u16 NextPollBitmap = 0xFFFF;
    s32 RoundCount = 0;
    s32 RoundLimit = 0;
    bool ChildKick = false;
    bool ChildPrepareWait = false;
    bool ChildVSync = true;
    u64 AcceptedRequests = 0;
    u64 PublishedCallbacks = 0;
    u64 ReceivedOperations = 0;
    u64 RejectedEpochs = 0;
    MAC Mac{};
    std::array<Peer, 16> Peers{};
    std::array<u16, 8> Sequences{};
    std::array<u8, 64> ParentParameters{};
    std::array<u8, 28> MPParameters{};
    std::array<u16, 4> LifeTimes{};
    std::array<u16, 16> CAMLifeTimes{};
    Bytes GameInfo;
    Bytes ParentBSS;
    Request ScanRequest;
    Request ConnectRequest;
    std::deque<Request> Requests;
    std::deque<Callback> Callbacks;
    std::deque<Operation> Operations;
    Callback CurrentCallback;
    PreparedPacket Prepared;
    MPRound Round;
    RoundIntent Intent;
    std::optional<u64> CheckedVBlankFrame;

    explicit Impl(WMConsole& console) : Console(console) { CAMLifeTimes.fill(0xFFFF); }

    bool Range(u32 address, u32 size) const
    {
        return address >= 0x02000000 && address < 0x03000000
            && size <= 0x03000000 - address
            && size <= Console.MainRAMMask + 1
            && (address & Console.MainRAMMask) <= Console.MainRAMMask + 1 - size;
    }

    const u8* Memory(u32 address) const { return Console.MainRAM + (address & Console.MainRAMMask); }
    u16 Read16(u32 address) const { return Range(address, 2) ? U16(Memory(address)) : 0; }
    u32 Read32(u32 address) const { return Range(address, 4) ? U32(Memory(address)) : 0; }
    void Write16(u32 address, u16 value) { Console.ARM7Write16(address, value); }
    void Write32(u32 address, u32 value) { Console.ARM7Write32(address, value); }
    void Write(u32 address, const u8* data, u32 size)
    {
        for (u32 i = 0; i < size; i++) Console.ARM7Write8(address + i, data[i]);
    }
    bool Copy(Bytes& output, u32 address, u32 size, u32 limit)
    {
        if (size > limit || (size && !Range(address, size))) return false;
        if (size) output.assign(Memory(address), Memory(address) + size);
        else output.clear();
        return true;
    }
    void State(u16 state) { if (Status) Write16(Status, state); }
    u16 State() const { return Status ? Read16(Status) : 0; }
    const WMABI& Layout() const { return ABIFor(Profile); }
    u32 MPOffset() const { return Layout().MP; }
    u32 RXOffset() const { return Layout().RX; }
    u32 RXSizeOffset() const { return Layout().RXSize; }
    u32 RXSelectorOffset() const { return Layout().RXSelector; }
    void EncodeMAC(u8* output, const MAC& mac) const
    {
        for (u32 i = 0; i < 6; i++) output[i] = mac[Layout().Firmware ? (i ^ 1) : i];
    }
    MAC ReadMAC(const u8* input) const
    {
        MAC mac{};
        std::copy_n(input, mac.size(), mac.begin());
        return mac;
    }
    void StoreMAC(u32 address, const MAC& mac)
    {
        MAC encoded{};
        EncodeMAC(encoded.data(), mac);
        Write(address, encoded.data(), encoded.size());
    }

    void UpdateMPReady()
    {
        u16 ready = 0;
        for (unsigned id = 0; id < 16; id++)
            if (Peers[id].Connected && Peers[id].MP && Negotiated(id))
                ready |= Parent ? (1u << Peers[id].AID) : 1;
        ReadyBitmap = ready;
        if (!Layout().Firmware) Write16(Status + Layout().Ready, ready);
    }

    void LastSeen(u16 aid, u64 timestamp)
    {
        if (!Layout().LegacyRetail() || aid >= 16) return;
        Write32(Status + Layout().LastSeen + aid * 8, timestamp);
        Write32(Status + Layout().LastSeen + aid * 8 + 4, timestamp >> 32);
    }

    u32 BranchTarget(u32 address) const
    {
        const u32 instruction = Read32(address);
        if ((instruction & 0xFF000000) != 0xEB000000) return 0;
        s32 displacement = instruction & 0xFFFFFF;
        if (instruction & 0x800000) displacement -= 0x1000000;
        const s64 target = s64(address) + 8 + s64(displacement) * 4;
        return target >= 0x02000000 && target < 0x03000000 ? static_cast<u32>(target) : 0;
    }

    bool HasLegacyMPProducer(u32 global) const
    {
        unsigned found = 0;
        for (u32 offset = 0; offset + 0x180 <= Console.MainRAMMask + 1; offset += 4)
        {
            const u8* code = Console.MainRAM + offset;
            if (U32(code) != 0xE92D41F0 || U32(code + 4) != 0xE24DD020
                || U32(code + 0x58) != 0xE28000C2 || U32(code + 0x74) != 0xE1D10CB2
                || U32(code + 0xCC) != 0xE216003F || U32(code + 0xFC) != 0xE214001F
                || U32(code + 0x158) != 0xE1A030A6 || U32(code + 0x15C) != 0xE3A0000E
                || U32(code + 0x160) != 0xE3A0100A) continue;
            const u32 address = 0x02000000 + offset;
            const u32 getter = BranchTarget(address + 0x18);
            const u32 receive = BranchTarget(address + 0xB4);
            const u32 transmit = BranchTarget(address + 0xE4);
            if (Read32(getter) != 0xE59F0004 || Read32(getter + 4) != 0xE5900000
                || Read32(getter + 8) != 0xE12FFF1E || Read32(getter + 12) != global
                || Read32(receive + 0x9C) != 0xE1D059B0 || Read32(transmit + 0x6C) != 0xE1D008BE) continue;
            ++found;
        }
        return found == 1;
    }

    bool Detect(const Request& request)
    {
        if ((request.API() != 0 && request.API() != 3)) return false;
        const u32 wm7 = request.Arg(4), status = request.Arg(8), reply = request.Arg(12);
        if ((wm7 | status | reply) & 3) return false;
        if (!Range(wm7, 0x100) || !Range(status, 0xA00) || !Range(reply, 0x100)) return false;
        u8 foundProfile = 0;
        u32 foundOwner = 0;
        unsigned found = 0;
        for (u32 offset = 0; offset + 0x3C8 <= Console.MainRAMMask + 1; offset += 2)
        {
            const u8* code = Console.MainRAM + offset;
            u8 profile = 0;
            u32 owner = 0;
            if ((offset & 3) == 0 && U32(code) == 0xE92D4FF8 && U32(code + 4) == 0xE24DD008
                && U32(code + 8) == 0xE59F0388 && U32(code + 0x10) == 0xE5904004
                && U32(code + 0xA8) == 0xE59100CC && U32(code + 0xB4) == 0xE591010C
                && U32(code + 0xFC) == 0xE59A101C && U32(code + 0x190) == 0xE5901018)
            {
                profile = 1;
                const u32 global = U32(code + 0x398);
                if (Range(global, 8)) owner = Read32(global + 4);
            }
            else if ((offset & 3) == 0 && offset + 0x3CC <= Console.MainRAMMask + 1
                && U32(code) == 0xE92D4FF0 && U32(code + 4) == 0xE24DD00C
                && U32(code + 8) == 0xE59F03B8 && U32(code + 0x10) == 0xE5908000
                && U32(code + 0xAC) == 0xE59100CC && U32(code + 0xB8) == 0xE591010C
                && U32(code + 0xD0) == 0xE1D117B2 && U32(code + 0x150) == 0xE1D117B2
                && U32(code + 0x100) == 0xE59A101C && U32(code + 0x1A4) == 0xE5901018)
            {
                profile = 1;
                const u32 global = U32(code + 0x3C8);
                if (Range(global, 4)) owner = Read32(global);
            }
            else if (U32(code) == 0xB081B5F0 && U32(code + 4) == 0x2A001C0D
                && U16(code + 0x5C) == 0x30B0 && U16(code + 0x66) == 0x32F0
                && U16(code + 0x96) == 0x6969 && U16(code + 0xD4) == 0x6941)
            {
                profile = 2;
                const u32 global = U32(code + 0x190);
                if (Range(global, 4)) owner = Read32(global);
            }
            else if ((offset & 3) == 0 && U32(code) == 0xE92D47F0 && U32(code + 4) == 0xE24DD008
                && U32(code + 8) == 0xE3520000 && U32(code + 12) == 0xE1A09001
                && U32(code + 0x1C) == 0xE59F0360 && U32(code + 0xEC) == 0xE1D114B6
                && U32(code + 0x17C) == 0xE1D114B6 && U32(code + 0x1E0) == 0xE5901018)
            {
                const u32 slot = U32(code + 0xCC);
                if (slot == 0xE59210C4 && U32(code + 0xD8) == 0xE5921104
                    && U32(code + 0x2F8) == 0xE59200C4 && U32(code + 0x304) == 0xE5921104) profile = 3;
                else if (slot == 0xE59210C0 && U32(code + 0xD8) == 0xE5921100
                    && U32(code + 0x2F8) == 0xE59200C0 && U32(code + 0x304) == 0xE5921100) profile = 4;
                const u32 global = U32(code + 0x384);
                if (profile && Range(global, 4) && !(global & 3) && HasLegacyMPProducer(global))
                    owner = Read32(global);
            }
            if (!profile || !Range(owner, 0x154) || (owner & 3)) continue;
            const auto& abi = ABIFor(profile);
            if (wm7 != owner + abi.WM7Base
                || status != owner + abi.StatusBase
                || reply != owner + abi.ReplyBase
                || Read32(owner) != wm7 || Read32(owner + 4) != status
                || Read32(owner + 0x10) != reply) continue;
            ++found;
            foundProfile = profile;
            foundOwner = owner;
        }
        if (found != 1 || (request.API() == 3 && !ABIFor(foundProfile).Modern && !ABIFor(foundProfile).SplitPower)) return false;
        Profile = foundProfile;
        WM9 = foundOwner;
        WM7 = wm7;
        Status = status;
        Reply = reply;
        return true;
    }

    Callback Result(u16 api, u16 error = 0) const
    {
        Callback result;
        Put16(result.Data.data(), api);
        Put16(result.Data.data() + 2, error);
        return result;
    }

    bool Queue(Callback callback)
    {
        if (Callbacks.size() >= MaxCallbacks)
        {
            Fail("callback capacity");
            return false;
        }
        if (U16(callback.Data.data()) == 38 && ScanToken)
        {
            callback.ScanToken = ScanToken;
            callback.ScanEpoch = ScanEpoch;
            callback.ScanOwner = ScanOwner;
        }
        Callbacks.push_back(std::move(callback));
        return true;
    }

    void Fail(const char* cause)
    {
        if (Failed) return;
        Failed = true;
        ClearBeaconPhase();
        Intent = {};
        InvalidateScanEx();
        Platform::Log(Platform::LogLevel::Error,
            "WMHLE failure cause=%s epoch=%llu state=%u requests=%llu callbacks=%llu received=%llu stale=%llu\n",
            cause, static_cast<unsigned long long>(Epoch), State(),
            static_cast<unsigned long long>(AcceptedRequests), static_cast<unsigned long long>(PublishedCallbacks),
            static_cast<unsigned long long>(ReceivedOperations), static_cast<unsigned long long>(RejectedEpochs));
    }

    void NewEpoch()
    {
        ClearBeaconPhase();
        Intent = {};
        InvalidateScanEx();
        ScanExConfigured = false;
        ScanSSIDPrefix = 32;
        u64 observed = EpochCounter.load(std::memory_order_relaxed);
        do { Epoch = std::max(observed + 1, Now()); }
        while (!EpochCounter.compare_exchange_weak(observed, Epoch, std::memory_order_relaxed));
        NextOffer = 0;
        PresenceRecipients = 0;
        for (auto& peer : Peers)
        {
            peer.Ready = false;
            peer.TimingSupportSent = peer.TimingSupportReceived = false;
            peer.NextHeartbeat = 0;
        }
    }

    u64 Trace(TraceMetadata metadata, const u8* data = nullptr, u32 length = 0,
              const u8* extra = nullptr, u32 extraLength = 0) const
    {
        if (!Console.IsWMTraceActive()) return ~u64{0};
        metadata.Valid |= TraceMetadata::OwnerValid | TraceMetadata::ScanValid;
        metadata.Session = Session;
        metadata.Epoch = Epoch;
        metadata.LocalID = static_cast<u32>(LocalID);
        metadata.WM9 = WM9;
        metadata.ScanToken = ScanToken;
        metadata.ScanEpoch = ScanEpoch;
        metadata.ScanOwner = ScanOwner;
        metadata.ScanDeadline = ScanDeadline;
        metadata.NextBeacon = NextBeacon;
        metadata.CallbackCount = Callbacks.size();
        for (unsigned id = 0; id < Peers.size(); ++id)
            if (Negotiated(id)) metadata.NegotiatedMask |= 1u << id;
        metadata.State = State();
        metadata.Mode = static_cast<u32>(Mode);
        metadata.ServiceFlags = (Capable ? 1u : 0u) | (Initialized ? 2u : 0u) | (Failed ? 4u : 0u)
            | (Parent ? 8u : 0u) | (ScanPending ? 16u : 0u) | (Terminal ? 128u : 0u);
        metadata.ScanMask = ScanMask;
        metadata.ScanChannel = ScanChannel;
        metadata.ScanDwell = ScanRequest.Half(10);
        metadata.ScanInterval = ScanInterval;
        metadata.ScanElapsed = ScanElapsed;
        metadata.ScanCount = ScanCount;
        metadata.ScanBytes = ScanBytes;
        metadata.ScanBudget = ScanBudget;
        metadata.Profile = Profile;
        if (metadata.PeerID < Peers.size())
        {
            const auto& peer = Peers[metadata.PeerID];
            metadata.Valid |= TraceMetadata::PeerValid;
            metadata.PeerEpoch = peer.Epoch;
            metadata.ServiceFlags |= (peer.Ready ? 32u : 0u)
                | ((Present & (1u << metadata.PeerID)) ? 64u : 0u) | (peer.Connected ? 256u : 0u);
        }
        return Console.TraceWMServiceEvent(metadata, data, length, extra, extraLength);
    }

    static bool TraceMessage(u32 type)
    {
        return type == static_cast<u32>(Message::Offer) || type == static_cast<u32>(Message::Ready)
            || type == static_cast<u32>(Message::Reject) || type == static_cast<u32>(Message::Advertisement)
            || type == static_cast<u32>(Message::ScanProbe) || type == static_cast<u32>(Message::ScanResponse)
            || type == static_cast<u32>(Message::BeaconTimingSupport);
    }

    void TraceTransmit(TraceReason reason, unsigned id, Message type, const u8* data, u32 length, u32 declared) const
    {
        if (!Console.IsWMTraceActive() || !TraceMessage(static_cast<u32>(type))) return;
        TraceMetadata metadata(TraceStage::Transmit, reason, id);
        metadata.Message = static_cast<u32>(type);
        metadata.DeclaredLength = declared;
        metadata.Valid |= TraceMetadata::LengthValid;
        if (data) metadata.Valid |= TraceMetadata::PacketValid;
        Trace(metadata, data, length);
    }

    u64 TracePacket(TraceStage stage, TraceReason reason, const WMServicePacket& packet, u64 cause = ~u64{0}) const
    {
        if (!Console.IsWMTraceActive()) return ~u64{0};
        const u32 type = packet.Length >= 7 ? packet.Data[6] : 0;
        if (packet.Length >= 24 && !TraceMessage(type)) return ~u64{0};
        TraceMetadata metadata(stage, reason, packet.SenderID, cause);
        metadata.Connection = packet.Connection;
        metadata.Message = type;
        metadata.DeclaredLength = packet.Length;
        metadata.Valid |= TraceMetadata::ConnectionValid | TraceMetadata::LengthValid | TraceMetadata::PacketValid;
        if (packet.Length > packet.Data.size()) metadata.RecordFlags |= WMServiceTrace::InvalidData;
        return Trace(metadata, packet.Data.data(), std::min<u32>(packet.Length, packet.Data.size()));
    }

    void TraceGameInfo(const Request& request, const Bytes& payload) const
    {
        if (!Console.IsWMTraceActive()) return;
        TraceMetadata metadata(TraceStage::GameInfo, TraceReason::Applied);
        metadata.Valid |= TraceMetadata::RequestValid | TraceMetadata::LengthValid;
        metadata.RequestAddress = request.Address;
        metadata.API = request.API();
        metadata.DeclaredLength = payload.size();
        Trace(metadata, payload.data(), payload.size(), GameInfo.data(), GameInfo.size());
    }

    void TraceScan(TraceStage stage, TraceReason reason, unsigned id = ~u32{0}, const u8* data = nullptr,
                   u32 length = 0, u64 cause = ~u64{0}) const
    {
        if (!Console.IsWMTraceActive()) return;
        TraceMetadata metadata(stage, reason, id, cause);
        metadata.Valid |= TraceMetadata::RequestValid | TraceMetadata::LengthValid;
        metadata.RequestAddress = ScanRequest.Address;
        metadata.API = ScanRequest.API();
        metadata.DeclaredLength = length;
        Trace(metadata, data, length);
    }

    bool Send(unsigned id, Message type, const u8* data = nullptr, u16 size = 0)
    {
        if (!Capable) { TraceTransmit(TraceReason::Incapable, id, type, nullptr, 0, 24u + size); return false; }
        if (id >= 16) { TraceTransmit(TraceReason::PeerID, id, type, nullptr, 0, 24u + size); return false; }
        if (size > 2024) { TraceTransmit(TraceReason::TooLong, id, type, nullptr, 0, 24u + size); return false; }
        if (!(Present & (1u << id))) { TraceTransmit(TraceReason::Absent, id, type, nullptr, 0, 24u + size); return false; }
        std::array<u8, 2048> packet{};
        Put32(packet.data(), Magic);
        Put16(packet.data() + 4, WMServiceVersion);
        packet[6] = static_cast<u8>(type);
        Put64(packet.data() + 8, Epoch);
        Put64(packet.data() + 16, type == Message::Offer ? 0 : Peers[id].Epoch);
        if (size) std::memcpy(packet.data() + 24, data, size);
        const bool sent = (Platform::MP_WMSend(1u << id, packet.data(), 24 + size, Console.UserData) & (1u << id)) != 0;
        if (sent && type != Message::Offer && type != Message::Ready && type != Message::Reject && type != Message::Heartbeat
            && type != Message::BeaconTimingSupport)
        {
            Peers[id].NextHeartbeat = Now() + HeartbeatIntervalUs;
            if (type == Message::Advertisement || type == Message::Connect || type == Message::Accepted)
                PresenceRecipients |= 1u << id;
        }
        TraceTransmit(sent ? TraceReason::Enqueued : TraceReason::EnqueueFailed, id, type, packet.data(), 24u + size, 24u + size);
        return sent;
    }

    void SendTimingSupport(unsigned id)
    {
        auto& peer = Peers[id];
        if (Profile == 3 && peer.Profile == 3 && !peer.TimingSupportSent)
            peer.TimingSupportSent = Send(id, Message::BeaconTimingSupport);
    }

    bool TimingLink(const Peer& peer) const
    {
        return Profile == 3 && peer.Profile == 3 && peer.TimingSupportSent && peer.TimingSupportReceived
            && peer.Ready && peer.Connected && peer.MP && peer.Connection && peer.MPGeneration && MP && MPGeneration;
    }

    void Offer(unsigned id)
    {
        std::array<u8, 8> offer{};
        offer[0] = Profile;
        std::copy(Mac.begin(), Mac.end(), offer.begin() + 2);
        Send(id, Message::Offer, offer.data(), offer.size());
    }

    bool Negotiated(unsigned id) const
    {
        return id < 16 && Peers[id].Ready && Peers[id].Epoch && (Present & (1u << id));
    }

    void Fatal(const char* cause)
    {
        if (Terminal) return;
        Terminal = true;
        Fail(cause);
        Platform::Log(Platform::LogLevel::Error,
            "WMHLE terminal cause=%s session=%llu epoch=%llu wm9=%08X fifo=%u word=%08X\n",
            cause, static_cast<unsigned long long>(Session), static_cast<unsigned long long>(Epoch),
            WM9, Console.Level(1), (Console.Level(1) == 0) ? 0 : Console.Peek());
    }

    void Reject()
    {
        if (Failed) return;
        Fail("peer rejected strict HLE owner");
        for (unsigned id = 0; id < 16; id++)
            if (Peers[id].Epoch) Send(id, Message::Reject);
    }

    void Capture(u32 word)
    {
        if ((word & 0x3F) != 0xA || Mode == Backend::LLE || Terminal) return;
        if (Mode == Backend::Undecided)
        {
            const auto info = Platform::MP_WMInfo(Console.UserData);
            if (!info.Session) { Mode = Backend::LLE; return; }
            Session = info.Session;
            LocalID = info.LocalID;
            Present = info.Peers;
            Capable = !info.Terminal;
            Mode = Backend::HLE;
            if (info.Terminal) { Fatal("strict transport violation"); return; }
        }
        Request request;
        request.Word = word;
        request.Address = word >> 6;
        request.Valid = Range(request.Address, request.Data.size());
        if (request.Valid) std::memcpy(request.Data.data(), Memory(request.Address), request.Data.size());
        if (!Profile && !Closing)
        {
            if (!request.Valid || LocalID < 0 || !Detect(request))
            {
                Fatal("unsupported local WM owner");
                return;
            }
            Mac = Console.Mac();
            AllowedChannels = Console.Channels() & 0x3FFE;
            NewEpoch();
        }
        else if (!Closing && (request.API() == 0 || request.API() == 3)
            && (request.Arg(4) != WM7 || request.Arg(8) != Status || request.Arg(12) != Reply))
        {
            Fatal("WM owner replaced before retirement");
            return;
        }
        if (!request.Valid) Fail("request address");
        if (request.Valid)
        {
            switch (request.API())
            {
            case 7:
                request.Valid = Copy(request.Payload, request.Arg(4), 64, 64);
                if (request.Valid)
                    request.Valid = Copy(request.Extra, U32(request.Payload.data()), U16(request.Payload.data() + 4), 112);
                break;
            case 12: request.Valid = Copy(request.Payload, request.Arg(4), 192, 192); break;
            case 15:
                request.Valid = request.Arg(8) <= 512
                    && Copy(request.Payload, request.Arg(4), (request.Arg(8) + 1u) & ~1u, 512);
                break;
            case 24: request.Valid = Copy(request.Payload, request.Arg(4), request.Half(8), 112); break;
            default: break;
            }
        }
        if (Requests.size() >= 16) { Fail("request capture capacity"); return; }
        Requests.push_back(std::move(request));
    }

    bool BlocksARM7() const
    {
        return Mode == Backend::HLE
            && Console.Level(1) != 0 && (Console.Peek() & 0x3F) == 0xA;
    }

    void Drain()
    {
        if (Mode != Backend::HLE || Terminal || Closing || !(Console.Control(1) & 0x8000)) return;
        while (BlocksARM7() && Callbacks.size() < MaxCallbacks - 64)
        {
            const u32 word = Console.Peek();
            if (Requests.empty() || Requests.front().Word != word) { Fail("FIFO capture order"); return; }
            if (ScanToken || (Profile == 1 && Requests.front().API() == 38 && ScanPending)) break;
            Request request = std::move(Requests.front());
            Requests.pop_front();
            Console.Pop();
            ++AcceptedRequests;
            if (Layout().LegacyRetail()) Write32(Status + 4, 1);
            const bool scanDispatch = Profile == 1 && request.Valid && request.API() == 38;
            if (scanDispatch)
            {
                ScanRequest = request;
                ScanToken = NextScanToken++;
                if (!ScanToken) { Fail("scan generation exhausted"); break; }
                ScanEpoch = Epoch;
                ScanOwner = WM9;
            }
            Handle(request);
            if (Layout().LegacyRetail()) Write32(Status + 4, 0);
            if (!Layout().Firmware && request.Valid && !scanDispatch) Write16(request.Address, request.API() | 0x8000);
            if ((Console.Level(1) == 0) && (Console.Control(0) & 4)) Console.Interrupt(0, 17);
            if (Failed || Closing) break;
        }
        if (Console.Level(1) != 0 && !BlocksARM7() && (Console.Control(1) & 0x400))
            Console.Interrupt(1, 18);
    }

    void Publish()
    {
        if (InFlight)
        {
            if (Read16(CallbackFlag) & 1) return;
            const bool ended = CurrentCallback.End;
            if (CurrentCallback.RoundEvent == 1 && !Layout().Modern
                && CurrentCallback.RoundEpoch == Epoch && CurrentCallback.RoundOwner == MPGeneration)
                ReleaseRound(CurrentCallback.RoundID);
            CurrentCallback = {};
            InFlight = false;
            if (ended)
            {
                Initialized = false;
                Closing = false;
                Status = WM7 = WM9 = Reply = 0;
                Profile = 0;
            }
        }
        if (ScanToken && (ScanEpoch != Epoch || ScanOwner != WM9)) InvalidateScanEx();
        while (!Callbacks.empty() && Callbacks.front().ScanToken && !OwnsScan(Callbacks.front()))
            Callbacks.pop_front();
        if (Callbacks.empty() || !Reply || (Read16(CallbackFlag) & 1)
            || !(Console.Control(1) & 0x8000) || (Console.Level(0) == 16)) return;
        CurrentCallback = std::move(Callbacks.front());
        Callbacks.pop_front();
        if (!CurrentCallback.Receive.empty())
            Write(CurrentCallback.ReceiveBase, CurrentCallback.Receive.data(), CurrentCallback.Receive.size());
        const u16 api = U16(CurrentCallback.Data.data());
        const u16 state = U16(CurrentCallback.Data.data() + 4);
        if (api == 14 && state >= 11 && state <= 13)
        {
            Write(Reply, CurrentCallback.Data.data(), 6);
            Write(Reply + 8, CurrentCallback.Data.data() + 8, 4);
            if (state == 13 && CurrentCallback.AckMetadata)
                Write(Reply + 12, CurrentCallback.Data.data() + 12, 22);
        }
        else if (Layout().SplitPower && api >= 3 && api <= 6)
            Write(Reply, CurrentCallback.Data.data(), U16(CurrentCallback.Data.data() + 2) == 1 ? 8 : 4);
        else if (Layout().LegacyRetail() && api == 33)
            Write(Reply, CurrentCallback.Data.data(), U16(CurrentCallback.Data.data() + 2) == 1 ? 8 : 4);
        else if (Layout().LegacyRetail() && api == 35)
            Write(Reply, CurrentCallback.Data.data(), 4);
        else if (Layout().LegacyRetail() && U16(CurrentCallback.Data.data()) == 12)
        {
            Write(Reply, CurrentCallback.Data.data(), 4);
            if (!U16(CurrentCallback.Data.data() + 2))
            {
                const u16 state = U16(CurrentCallback.Data.data() + 8);
                Write(Reply + 8, CurrentCallback.Data.data() + 8, state == 9 ? 6 : state == 7 ? 4 : 2);
            }
        }
        else Write(Reply, CurrentCallback.Data.data(), CurrentCallback.Data.size());
        Write16(CallbackFlag, Read16(CallbackFlag) | 1);
        InFlight = true;
        ++PublishedCallbacks;
        Console.Reply((Reply << 6) | 0xA);
        if (CurrentCallback.ScanToken && OwnsScan(CurrentCallback))
        {
            Write16(ScanRequest.Address, ScanRequest.API() | 0x8000);
            ScanToken = 0;
            ScanRequest = {};
        }
        if (CurrentCallback.RoundEpoch == Epoch && CurrentCallback.RoundOwner == MPGeneration)
        {
            if (CurrentCallback.RoundEvent == 1 && Layout().Modern) ReleaseRound(CurrentCallback.RoundID);
            if (CurrentCallback.RoundEvent == 2) ChildKick = true;
        }
    }

    void InitializeLegacyPower();
    void Handle(const Request& request);
    void Network(const WMServicePacket& packet);
    void Tick();
    void PollHost(u64 vblankDeadline = 0);
    bool ClaimVBlankWait(u64 deadline);
    void ReceivePackets(bool host);
    bool CanPollHost() const
    {
        return Mode == Backend::HLE && MP && !Failed;
    }
    void Disconnect(unsigned id, bool notify, std::optional<u16> reason = 3);
    void Complete(Operation& operation, u16 error);
    void CancelOperations(u16 error);
    void BuildGameInfo(const Bytes& payload);
    void BuildBSS(u8* bss) const;
    void Advertisement();
    bool OwnsScan(const Callback& callback) const
    {
        return ScanToken && callback.ScanToken == ScanToken && callback.ScanEpoch == Epoch
            && ScanEpoch == Epoch && callback.ScanOwner == WM9 && ScanOwner == WM9;
    }
    void InvalidateScanEx()
    {
        if (ScanToken || ScanPending || ScanCount) TraceScan(TraceStage::ScanWindow, TraceReason::ScanInvalidated);
        if (ScanToken) ScanPending = false;
        ScanToken = 0;
        ScanCount = ScanBytes = 0;
        ScanData = Bytes{};
    }
    void PruneScanEx();
    void ObserveScanEx(unsigned id, const u8* data, u16 size, u64 cause = ~u64{0});
    void ProbeScanEx();
    void AdvanceScanEx();
    void FinishScanEx();
    void TryScan(unsigned received = 16, u64 cause = ~u64{0});
    void TryConnect();
    void ReceiveIntent(unsigned id, const u8* data, u16 size, bool fallback);
    void PromoteIntent(bool fallback = false);
    void ReceiveCancel(unsigned id, const u8* data, u16 size);
    void ReceivePoll(unsigned id, const u8* data, u16 size);
    void ReceiveReply(unsigned id, const u8* data, u16 size);
    void ReceiveRoundAck(unsigned id, const u8* data, u16 size);
    u16 ApplyParameters(const u8* parameters, std::array<u8, 28>* old);
    void SetSizes();
    void PumpOperations();
    bool CanPrepareChild() const
    {
        return ChildKick && !Prepared.ID && !Round.Packet.ID && Console.Timestamp() >= NextPrepareTick;
    }
    PreparedPacket PreparePacket(u16 pollBitmap);
    bool FlushPacket(const PreparedPacket& packet, u16 failed, bool timeout);
    void PortCallbacks(unsigned id, u16 header, const Bytes& data, u32 receiveBase, u32 dataOffset);
    void StartRound();
    void TrySendRound();
    bool CancelRoundPeer(unsigned id);
    void FinishRound();
    void FinishChild(u16 failed, bool timeout, const u8* metadata = nullptr);
    void ReleaseRound(u64 id);
    void UpdateRoundBudget();
    void WaitForMP(u64 vblankDeadline = 0);
    u64 OperationLifetime() const;
    void ClearRounds();
    void ClearBeaconPhase();
    void ApplyDueBeacons();
    void UpdateBeaconPhase(u16 previous, u16 current, bool wrapped);
    void ReceiveBeaconTiming(unsigned id, const u8* data);
    Bytes RoundHeader(unsigned id, u64 round, u64 preparation, u16 header, u16 flags, u16 size) const;
};

void WMService::Impl::BuildGameInfo(const Bytes& payload)
{
    const u32 shift = Layout().Firmware ? 2 : 0;
    GameInfo.assign(16 + payload.size(), 0);
    Put16(GameInfo.data(), 1);
    GameInfo[2] = Layout().Modern ? 1 : 0;
    Put32(GameInfo.data() + 4, U32(ParentParameters.data() + 8 - shift));
    Put16(GameInfo.data() + 8, U16(ParentParameters.data() + 12 - shift));
    GameInfo[10] = payload.size();
    for (u32 bit = 0; bit < (Layout().Modern ? 3u : 4u); bit++)
    {
        const u32 field = bit == 0 ? 0xE - shift : 0x10 + 2 * bit - shift;
        if (U16(ParentParameters.data() + field)) GameInfo[11] |= 1u << bit;
    }
    Put16(GameInfo.data() + 12, U16(ParentParameters.data() + 0x34 - shift));
    Put16(GameInfo.data() + 14, U16(ParentParameters.data() + 0x36 - shift));
    std::copy(payload.begin(), payload.end(), GameInfo.begin() + 16);
}

void WMService::Impl::SetSizes()
{
    if (Layout().Modern)
    {
        Write16(Status + 0x30, ParentSize);
        Write16(Status + 0x32, ChildSize);
        Write16(Status + 0x34, MaxParentSize);
        Write16(Status + 0x36, MaxChildSize);
        Write16(Status + 0x38, Parent ? ParentSize + 4 : ChildSize + 2);
        Write16(Status + 0x3A, Parent ? ChildSize + 2 : ParentSize + 4);
        Write16(Status + 0x3C, Parent ? MaxParentSize + 4 : MaxChildSize + 2);
        Write16(Status + 0x3E, Parent ? MaxChildSize + 2 : MaxParentSize + 4);
    }
    else if ((Parent || Layout().LegacyRetail()) && (State() == 7 || State() == 8 || State() == 9 || State() == 10))
    {
        const bool keySharing = Parent ? U16(ParentParameters.data() + (Layout().Firmware ? 0x12 : 0x14)) != 0
            : ConnectRequest.Payload.size() == 192 && (ConnectRequest.Payload[0x4B] & 4);
        const u16 parent = ParentSize + 4 + (keySharing ? 42 : 0);
        const u16 child = ChildSize + 2 + (keySharing ? 6 : 0);
        Write16(Status + Layout().EffectiveTX, Parent ? parent : child);
        Write16(Status + Layout().EffectiveRX, Parent ? child : parent);
    }
    Put16(MPParameters.data() + 10, ParentSize);
    Put16(MPParameters.data() + 12, ChildSize);
}

u16 WMService::Impl::ApplyParameters(const u8* parameters, std::array<u8, 28>* old)
{
    if (old)
    {
        *old = MPParameters;
        Put32(old->data(), 0x3FFF);
        Put16(old->data() + 4, U16(MPParameters.data() + 6));
        Put16(old->data() + 8, U16(MPParameters.data() + 6));
    }
    u32 mask = U32(parameters);
    u16 error = 0;
    if (mask & ~0x3FFFu) return 6;
    if (MP && (mask & 0x2C00)) { mask &= ~0x2C00u; error = 3; }
    constexpr u16 offsets[] = {0x58,0x5A,0x5C,0x30,0x32,0x44,0x46,0x40,0x42,0x98,0x92,0x94,0x9A,0x9C};
    for (unsigned bit = 0; bit < 14; bit++)
    {
        if (!(mask & (1u << bit))) continue;
        const u32 offset = bit < 10 ? 4 + bit * 2 : 24 + bit - 10;
        u16 value = bit < 10 ? U16(parameters + offset) : parameters[offset];
        if (bit < 3 && value == 0) value = 16;
        if ((bit == 3 && ((value + 1u) & ~1u) > MaxParentSize)
            || (bit == 4 && ((value + 1u) & ~1u) > MaxChildSize)
            || ((bit == 5 || bit == 6) && value > 10000)
            || ((bit == 7 || bit == 8) && !(value <= 190 || (value >= 220 && value <= 262))))
        { error = 6; continue; }
        if (bit < 10) Put16(MPParameters.data() + offset, value);
        else MPParameters[offset] = value;
        Write16(Status + offsets[bit], value);
        if (bit == 3) ParentSize = value;
        if (bit == 4) ChildSize = value;
        if (bit == 5 || bit == 6)
        {
            const u64 tick = (33514ULL * value) / 65536;
            const u32 address = Status + (bit == 5 ? 0x48 : 0x50);
            Write32(address, tick);
            Write32(address + 4, tick >> 32);
        }
    }
    SetSizes();
    return error;
}

void WMService::Impl::BuildBSS(u8* bss) const
{
    std::fill_n(bss, 192, 0);
    Put16(bss, 96);
    std::copy(Mac.begin(), Mac.end(), bss + 4);
    Put16(bss + 10, 32);
    std::copy(GameInfo.begin() + 4, GameInfo.begin() + 10, bss + 12);
    Put16(bss + 0x2C, 2);
    Put16(bss + 0x2E, 3);
    Put16(bss + 0x30, 3);
    Put16(bss + 0x32, BeaconPeriod);
    Put16(bss + 0x36, Channel);
    Put16(bss + 0x3C, GameInfo.size());
    std::copy(GameInfo.begin(), GameInfo.end(), bss + 0x40);
}

void WMService::Impl::Advertisement()
{
    if (!Parent || GameInfo.size() < 16) return;
    ParentBSS.resize(192);
    BuildBSS(ParentBSS.data());
    bool sent = false;
    bool hasReceiver = false;
    std::array<u8, 230> extended{};
    if (BeaconPhaseValid)
    {
        std::copy(ParentBSS.begin(), ParentBSS.end(), extended.begin());
        Put16(extended.data() + 192, BeaconVTSF);
        Put64(extended.data() + 194, RadioClock.At(Console.Timestamp()));
        Put16(extended.data() + 202, 64 + GameInfo.size());
        Put16(extended.data() + 204, Read16(Status + 0x1E8) == 2 ? 0x14 : 0x0A);
        Put64(extended.data() + 214, MPGeneration);
    }
    for (unsigned id = 0; id < Peers.size(); id++)
    {
        if (!Negotiated(id)) continue;
        hasReceiver = true;
        const auto& peer = Peers[id];
        if (BeaconPhaseValid && TimingLink(peer))
        {
            Put64(extended.data() + 206, peer.Connection);
            Put64(extended.data() + 222, peer.MPGeneration);
            sent |= Send(id, Message::Advertisement, extended.data(), extended.size());
        }
        else sent |= Send(id, Message::Advertisement, ParentBSS.data(), ParentBSS.size());
    }
    if ((sent || (Layout().Firmware && !hasReceiver)) && BeaconIndication.value_or(Layout().Firmware))
    {
        auto result = Result(8);
        Put16(result.Data.data() + 8, 2);
        Queue(std::move(result));
    }
    NextBeacon = Console.Timestamp() + std::max<u64>(1, BeaconPeriod) * 1024 * 33514 / 1000;
}

void WMService::Impl::PruneScanEx()
{
    u16 count = 0, end = 0, bytes = 0;
    for (u16 i = 0; i < ScanCount; ++i)
    {
        auto observation = ScanObservations[i];
        const auto& peer = Peers[observation.PeerID];
        if (!Negotiated(observation.PeerID) || peer.Epoch != observation.Epoch
            || !std::equal(peer.Mac.begin(), peer.Mac.end(), ScanData.begin() + observation.Offset + 4)) continue;
        const u16 offset = (end + 3u) & ~3u;
        std::fill(ScanData.begin() + end, ScanData.begin() + offset, 0);
        std::memmove(ScanData.data() + offset, ScanData.data() + observation.Offset, observation.Size);
        observation.Offset = offset;
        ScanObservations[count++] = observation;
        end = offset + observation.Size;
        bytes += observation.Size;
    }
    if (!ScanData.empty()) std::fill(ScanData.begin() + end, ScanData.end(), 0);
    ScanCount = count;
    ScanBytes = bytes;
}

void WMService::Impl::ObserveScanEx(unsigned id, const u8* data, u16 size, u64 cause)
{
    const auto trace = [&](TraceReason reason) { TraceScan(TraceStage::ScanDecision, reason, id, data, size, cause); };
    if (!ScanPending) { trace(TraceReason::NoScan); return; }
    if (ScanRequest.API() != 38) { trace(TraceReason::ScanAPI); return; }
    if (Failed) { trace(TraceReason::Failed); return; }
    if (ScanEpoch != Epoch) { trace(TraceReason::ScanEpoch); return; }
    if (ScanOwner != WM9) { trace(TraceReason::ScanOwner); return; }
    if (Console.Timestamp() >= ScanDeadline) { trace(TraceReason::ScanExpired); return; }
    if (!Negotiated(id)) { trace(TraceReason::NotNegotiated); return; }
    if (size != 192) { trace(TraceReason::BSSSize); return; }
    const u32 length = u32(U16(data)) * 2;
    const u16 ssidLength = U16(data + 10), gameLength = U16(data + 0x3C);
    if (length < 64 || length > size) { trace(TraceReason::BSSLength); return; }
    if (ssidLength > 32) { trace(TraceReason::SSIDLength); return; }
    if (ssidLength < ScanRequest.Half(20)) { trace(TraceReason::SSIDMinimum); return; }
    if (gameLength < 16 || gameLength > length - 64) { trace(TraceReason::BSSGameLength); return; }
    if (U16(data + 0x36) != ScanChannel) { trace(TraceReason::ScanChannel); return; }
    if (!std::equal(Peers[id].Mac.begin(), Peers[id].Mac.end(), data + 4)) { trace(TraceReason::BSSPeerMAC); return; }
    MAC filter = ReadMAC(ScanRequest.Data.data() + 12);
    if (U16(filter.data()) != 0xFFFF) filter[0] &= ~1u;
    const bool wildcard = std::all_of(filter.begin(), filter.end(), [](u8 value) { return value == 0xFF; });
    if (!wildcard && filter != Peers[id].Mac) { trace(TraceReason::MACFilter); return; }
    const u16 prefix = std::min(ScanRequest.Half(20), ScanSSIDPrefix);
    if (!std::equal(data + 12, data + 12 + prefix, ScanRequest.Data.begin() + 22)) { trace(TraceReason::SSIDFilter); return; }
    PruneScanEx();
    for (u16 i = 0; i < ScanCount; ++i)
        if (std::equal(data + 4, data + 10, ScanData.begin() + ScanObservations[i].Offset + 4))
        { trace(TraceReason::Duplicate); return; }
    const u16 offset = ScanCount ? (ScanObservations[ScanCount - 1].Offset + ScanObservations[ScanCount - 1].Size + 3u) & ~3u : 0;
    if (ScanCount == ScanObservations.size()) { trace(TraceReason::ObservationFull); return; }
    if (length > ScanBudget - ScanBytes) { trace(TraceReason::Budget); return; }
    if (u32(offset) + length > ScanRequest.Half(8)) { trace(TraceReason::Capacity); return; }
    if (ScanData.empty()) ScanData.assign((u32(ScanRequest.Half(8)) + 1) & ~1u, 0);
    std::copy_n(data, length, ScanData.begin() + offset);
    auto& observation = ScanObservations[ScanCount++];
    observation = {Peers[id].Epoch, offset, static_cast<u16>(length), static_cast<u8>(id)};
    ScanBytes += length;
    if (ScanRequest.Half(20) && std::all_of(data + 12, data + 12 + ssidLength, [](u8 value) { return value == 0; }))
    {
        Put16(ScanData.data() + offset + 10, ScanRequest.Half(20));
        std::copy_n(ScanRequest.Data.data() + 22, 32, ScanData.begin() + offset + 12);
    }
    trace(TraceReason::ScanAccepted);
    if (ScanBudget - ScanBytes < 64 || ScanCount == ScanObservations.size()) FinishScanEx();
}

void WMService::Impl::ProbeScanEx()
{
    if (!ScanPending || (ScanRequest.Half(18) & 1) || Failed) return;
    std::array<u8, 52> probe{};
    Put64(probe.data(), ScanToken);
    Put16(probe.data() + 8, ScanChannel);
    Put16(probe.data() + 10, ScanRequest.Half(20));
    std::copy_n(ScanRequest.Data.data() + 12, 6, probe.begin() + 12);
    if (U16(probe.data() + 12) != 0xFFFF) probe[12] &= ~1u;
    std::copy_n(ScanRequest.Data.data() + 22, 32, probe.begin() + 20);
    for (unsigned id = 0; id < Peers.size(); ++id)
        if (Negotiated(id) && !Send(id, Message::ScanProbe, probe.data(), probe.size()))
        { Fail("scan probe enqueue"); return; }
}

void WMService::Impl::AdvanceScanEx()
{
    if (!ScanPending || Failed) return;
    if (!ScanToken || ScanEpoch != Epoch || ScanOwner != WM9)
    { InvalidateScanEx(); return; }
    if (Console.Timestamp() < ScanDeadline) return;
    ScanElapsed += ScanInterval;
    if (ScanElapsed >= ScanRequest.Half(10))
    {
        do { ++ScanChannel; } while (ScanChannel <= 13 && !(ScanMask & (1u << (ScanChannel - 1))));
        if (ScanChannel > 13) { FinishScanEx(); return; }
        ScanElapsed = 0;
    }
    ScanDeadline = Console.Timestamp() + (TicksPerMillisecond * ScanInterval / 64) * 64;
    TraceScan(TraceStage::ScanWindow, TraceReason::ScanAdvance);
    ProbeScanEx();
}

void WMService::Impl::FinishScanEx()
{
    if (!ScanPending || Failed || ScanEpoch != Epoch || ScanOwner != WM9) return;
    PruneScanEx();
    TraceScan(TraceStage::ScanWindow, TraceReason::ScanFinish);
    auto result = Result(38);
    Put16(result.Data.data() + 8, ScanCount ? 5 : 4);
    Put16(result.Data.data() + 10, ScanMask);
    Put16(result.Data.data() + 14, ScanCount);
    if (ScanCount)
    {
        for (u16 i = 0; i < ScanCount; ++i)
        {
            Put32(result.Data.data() + 16 + i * 4, ScanRequest.Arg(4) + ScanObservations[i].Offset);
            Put16(result.Data.data() + 80 + i * 2, 3);
        }
        result.ReceiveBase = ScanRequest.Arg(4);
        result.Receive = std::move(ScanData);
    }
    ScanData = Bytes{};
    ScanPending = false;
    Queue(std::move(result));
}

void WMService::Impl::TryScan(unsigned received, u64 cause)
{
    if (!ScanPending)
    {
        if (received < Peers.size())
            TraceScan(TraceStage::ScanDecision, TraceReason::NoScan, received, Peers[received].BSS.data(), Peers[received].BSS.size(), cause);
        return;
    }
    if (ScanRequest.API() == 38)
    {
        if (received < Peers.size()) ObserveScanEx(received, Peers[received].BSS.data(), Peers[received].BSS.size(), cause);
        else AdvanceScanEx();
        return;
    }
    const MAC filter = ReadMAC(ScanRequest.Data.data() + 10);
    const bool wildcard = std::all_of(filter.begin(), filter.end(), [](u8 value) { return value == 0xFF; });
    if (received < Peers.size() && Console.Timestamp() <= ScanDeadline)
    {
        const auto& peer = Peers[received];
        const u16 channel = U16(peer.BSS.data() + 0x36);
        if (channel == ScanRequest.Half(2) && (wildcard || filter == peer.Mac))
        {
            auto result = Result(10);
            Put16(result.Data.data() + 8, 5);
            EncodeMAC(result.Data.data() + 10, peer.Mac);
            Put16(result.Data.data() + 0x10, channel);
            Put16(result.Data.data() + 0x12, 3);
            Put16(result.Data.data() + 0x14, U16(peer.BSS.data() + 10));
            std::copy(peer.BSS.begin() + 12, peer.BSS.begin() + 44, result.Data.begin() + 0x16);
            const u16 length = U16(peer.BSS.data() + 0x3C);
            Put16(result.Data.data() + 0x36, length);
            std::copy(peer.BSS.begin() + 0x40, peer.BSS.begin() + 0x40 + length, result.Data.begin() + 0x38);
            result.Receive = peer.BSS;
            result.ReceiveBase = ScanRequest.Arg(4);
            Queue(std::move(result));
            ScanPending = false;
            return;
        }
    }
    if (Console.Timestamp() >= ScanDeadline)
    {
        auto result = Result(10);
        Put16(result.Data.data() + 8, 4);
        Queue(std::move(result));
        ScanPending = false;
    }
}

void WMService::Impl::TryConnect()
{
    if (!ConnectPending || ParentID >= 0 || !Initialized || Closing || Failed) return;
    const auto& request = ConnectRequest;
    const MAC parentMAC = ReadMAC(request.Payload.data() + 4);
    int selected = -1;
    bool awaitingAdvertisement = false;
    for (unsigned id = 0; id < 16; id++)
    {
        if (!(Present & (1u << id)) || Peers[id].Mac != parentMAC) continue;
        if (!Negotiated(id) || Peers[id].BSS.empty())
        {
            awaitingAdvertisement = true;
            continue;
        }
        if (Peers[id].BSS.size() == 192
            && U16(request.Payload.data() + 0x36) == U16(Peers[id].BSS.data() + 0x36)
            && std::equal(request.Payload.begin() + 0x44, request.Payload.begin() + 0x4A, Peers[id].BSS.begin() + 0x44)) selected = id;
    }
    if (selected < 0)
    {
        if (awaitingAdvertisement) return;
        if (Console.IsWMTraceActive())
        {
            const auto hex = [](const u8* bytes, size_t size) {
                std::array<char, 385> text{};
                constexpr char digits[] = "0123456789ABCDEF";
                for (size_t i = 0; i < std::min<size_t>(size, 192); i++)
                {
                    text[2 * i] = digits[bytes[i] >> 4];
                    text[2 * i + 1] = digits[bytes[i] & 15];
                }
                return text;
            };
            const auto requested = hex(request.Payload.data(), request.Payload.size());
            const auto parent = hex(parentMAC.data(), parentMAC.size());
            Platform::Log(Platform::LogLevel::Warn,
                "WMHLE connect-reject epoch=%llu tick=%llu profile=%u request=%08X bss=%08X present=%04X mac=%s payload=%s\n",
                static_cast<unsigned long long>(Epoch), static_cast<unsigned long long>(Console.Timestamp()),
                Profile, request.Address, request.Arg(4), Present, parent.data(), requested.data());
            for (unsigned id = 0; id < Peers.size(); id++)
            {
                const auto& peer = Peers[id];
                const auto mac = hex(peer.Mac.data(), peer.Mac.size());
                const auto bss = hex(peer.BSS.data(), peer.BSS.size());
                int firstDifference = -1, firstGameInfoDifference = -1;
                for (size_t offset = 0; offset < std::min(request.Payload.size(), peer.BSS.size()); offset++)
                    if (request.Payload[offset] != peer.BSS[offset])
                    {
                        if (firstDifference < 0) firstDifference = offset;
                        if (offset >= 0x40 && firstGameInfoDifference < 0) firstGameInfoDifference = offset;
                    }
                Platform::Log(Platform::LogLevel::Warn,
                    "WMHLE connect-peer epoch=%llu tick=%llu request=%08X peer=%u negotiated=%u ready=%u peer_epoch=%llu mac_match=%u mac=%s bss_size=%u first_diff=%d gameinfo_diff=%d payload=%s\n",
                    static_cast<unsigned long long>(Epoch), static_cast<unsigned long long>(Console.Timestamp()),
                    request.Address, id, Negotiated(id) ? 1u : 0u, peer.Ready ? 1u : 0u,
                    static_cast<unsigned long long>(peer.Epoch), peer.Mac == parentMAC ? 1u : 0u,
                    mac.data(), static_cast<unsigned>(peer.BSS.size()), firstDifference, firstGameInfoDifference, bss.data());
            }
        }
        ConnectPending = false;
        State(2);
        Queue(Result(12, 9));
        return;
    }
    ParentID = selected;
    ConnectionID = NextConnection++;
    std::array<u8, 44> data{};
    Put64(data.data(), ConnectionID);
    std::copy(Mac.begin(), Mac.end(), data.begin() + 8);
    std::copy(request.Payload.begin() + 0x44, request.Payload.begin() + 0x4A, data.begin() + 14);
    std::copy_n(request.Data.begin() + 8, 24, data.begin() + 20);
    if (!Send(selected, Message::Connect, data.data(), data.size()))
    { ConnectPending = false; State(2); Queue(Result(12, 1)); return; }
    State(6);
    auto result = Result(12);
    Put16(result.Data.data() + 8, 6);
    if (Queue(std::move(result)) && !Layout().Firmware)
        Write16(Status + Layout().PowerSave, request.Arg(0x20) != 0);
}

void WMService::Impl::Complete(Operation& operation, u16 error)
{
    auto result = Result(0x81, error);
    Put16(result.Data.data() + 8, 20);
    Put16(result.Data.data() + 10, operation.Port);
    Put16(result.Data.data() + 12, operation.RequestedDestinations);
    const u16 length = operation.Size;
    if (Layout().Firmware)
    {
        Put16(result.Data.data() + 14, length);
        Put32(result.Data.data() + 16, operation.DataAddress);
        Put32(result.Data.data() + 20, operation.Function);
        Put32(result.Data.data() + 24, operation.Argument);
        Put16(result.Data.data() + 28, operation.Sequence);
    }
    else
    {
        Put16(result.Data.data() + 14, operation.Destinations & ~operation.Sent);
        Put16(result.Data.data() + 16, operation.Sent);
        Put32(result.Data.data() + 20, operation.DataAddress);
        Put16(result.Data.data() + 24, length);
        Put16(result.Data.data() + 26, operation.Sequence);
        Put32(result.Data.data() + 28, operation.Function);
        Put32(result.Data.data() + 32, operation.Argument);
        if (Layout().Modern)
        {
            Put16(result.Data.data() + 36, Parent ? ParentSize : ChildSize);
            Put16(result.Data.data() + 38, Parent ? ChildSize : ParentSize);
        }
    }
    Queue(std::move(result));
}

void WMService::Impl::CancelOperations(u16 error)
{
    ClearRounds();
    for (auto& operation : Operations) Complete(operation, error);
    Operations.clear();
}

void WMService::Impl::Disconnect(unsigned id, bool notify, std::optional<u16> reason)
{
    if (id >= 16 || !Peers[id].Connected) return;
    auto& peer = Peers[id];
    if (notify)
    {
        std::array<u8, 10> data{};
        Put64(data.data(), peer.Connection);
        Put16(data.data() + 8, 3);
        Send(id, Message::Disconnect, data.data(), data.size());
    }
    auto result = Result(Parent ? 8 : 12);
    Put16(result.Data.data() + 8, 9);
    LastSeen(Parent ? peer.AID : 0, 0);
    if (Parent)
    {
        EncodeMAC(result.Data.data() + 10, peer.Mac);
        Put16(result.Data.data() + 16, peer.AID);
        if (Layout().Modern)
        {
            Put16(result.Data.data() + 44, ParentSize);
            Put16(result.Data.data() + 46, ChildSize);
        }
        if (!Layout().Modern)
        {
            Put16(result.Data.data() + 18, Layout().Firmware ? reason.value_or(0) : 3);
            StoreMAC(Status + Layout().ChildMACs + (peer.AID - 1) * 6, {});
            if (Layout().Firmware)
                Write16(Status + Layout().Ready, Read16(Status + Layout().Ready) & ~(1u << peer.AID));
        }
        Members &= ~(1u << peer.AID);
    }
    else
    {
        Put16(result.Data.data() + 10, OwnAID);
        if (Layout().Modern) EncodeMAC(result.Data.data() + 16, peer.Mac);
        Members = 0;
        MP = false;
        Write32(Status + MPOffset(), 0);
        if (!Layout().Modern)
        {
            Write16(Status + Layout().EffectiveTX, 0);
            Write16(Status + Layout().EffectiveRX, 0);
            Put16(result.Data.data() + 12, Layout().Firmware ? reason.value_or(0) : 3);
        }
        if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().Indications, 0);
        }
        RadioClock.Stop(Console.Timestamp());
        State(2);
        ParentID = -1;
    }
    Write16(Status + Layout().Members, Members);
    Write16(Status + Layout().Link, Members ? 3 : 0);
    peer.Connected = peer.MP = false;
    UpdateMPReady();
    if (reason) Queue(std::move(result));
    for (auto& operation : Operations)
    {
        if (!(operation.Pending & (1u << id)) || operation.Connections[id] != peer.Connection) continue;
        operation.Pending &= ~(1u << id);
        operation.Queued &= ~(1u << id);
        operation.Destinations &= ~(Parent ? (1u << peer.AID) : 1);
    }
    if (!Parent) ClearRounds();
    else if (Round.ID && !Round.Closing && (Round.Expected & (1u << id)))
    {
        Round.Remaining &= ~(1u << id);
        Round.Failed |= 1u << peer.AID;
        if (!Round.Remaining) FinishRound();
    }
}

void WMService::Impl::InitializeLegacyPower()
{
    LifeTime = 40;
    LifeTimes = {0xFFFF, 0xFFFF, 0xFFFF, 40};
    FrameLifeTime = 5;
    CAMLifeTimes.fill(0xFFFF);
    BeaconIndication = true;
    if (Layout().SplitPower)
    {
        Write16(Status + 0x1E8, 2);
        Write16(Status + 0x1EA, 1);
    }
    const u64 ticks = (33514ULL * LifeTime * 100) / 64;
    Write32(Status + Layout().Lifetime, ticks);
    Write32(Status + Layout().Lifetime + 4, ticks >> 32);
}

void WMService::Impl::Handle(const Request& request)
{
    const u16 api = request.API();
    if (!request.Valid || Failed) { Queue(Result(api, Failed ? 1 : 6)); return; }
    if (api != 0 && api != 3 && !Initialized) { Queue(Result(api, 3)); return; }
    Write16(Status + 2, api);
    switch (api)
    {
    case 0:
    case 3:
    {
        if (api == 3 && !Layout().Modern && !Layout().SplitPower) { Queue(Result(api, 4)); break; }
        if (Initialized) { Queue(Result(api, 3)); break; }
        Write32(WM7, Status);
        Write32(WM7 + 8, Reply);
        Write32(Status + 4, 0);
        Write32(Status + MPOffset(), 0);
        Write16(Status + Layout().Members, 0);
        Write16(Status + Layout().Link, 0);
        Write16(Status + Layout().AID, 0);
        if (!Layout().SplitPower || api == 0)
        {
            Write16(Status + Layout().AllowedChannels, AllowedChannels);
            Write16(Status + Layout().MPChannels, Layout().LegacyRetail() ? MPChannels(AllowedChannels) : AllowedChannels >> 1);
            StoreMAC(Status + Layout().LocalMAC, Mac);
        }
        State(api == 3 ? 1 : 2);
        Initialized = EverActive = true;
        if (Layout().Firmware) BeaconIndication.reset();
        MPParameters = {};
        Put32(MPParameters.data(), 0x3FFF);
        Put16(MPParameters.data() + 4, 1);
        Put16(MPParameters.data() + 6, 1);
        Put16(MPParameters.data() + 8, 6);
        Put16(MPParameters.data() + 14, 1000);
        Put16(MPParameters.data() + 18, 260);
        Put16(MPParameters.data() + 20, 240);
        if (Layout().Modern)
        {
            Write16(Status + Layout().PowerSave, 0);
            Write16(Status + Layout().Ready, 0);
            ApplyParameters(MPParameters.data(), nullptr);
        }
        else if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().PowerSave, 0);
            Write16(Status + Layout().Ready, 0);
            Write16(Status + Layout().Indications, 0);
            for (u16 aid = 0; aid < 16; aid++) LastSeen(aid, 0);
            if (Layout().SplitPower)
            {
                Write32(Status + 0x10, 0);
                Write32(Status + 0x14, 0);
                Write32(Status + 0x1C, 0);
                Write16(Status + Layout().Booleans, 0);
                Write16(Status + Layout().Booleans + 2, 0);
                Write16(Status + Layout().Booleans + 4, 0);
                Write16(Status + Layout().EffectiveTX, 0);
                Write16(Status + Layout().EffectiveRX, 0);
                Write16(Status + 0xCA, 0);
                Write16(Status + 0x1EA, 1);
            }
            if (Profile == 3) Write16(Status + 0x1E8, 2);
            if (!Layout().SplitPower || api == 0) InitializeLegacyPower();
        }
        Queue(Result(api));
        Platform::Log(Platform::LogLevel::Info, "WMHLE initialized profile=%u epoch=%llu channels=%04X\n",
            Profile, static_cast<unsigned long long>(Epoch), AllowedChannels);
        break;
    }
    case 4:
    {
        if (!Layout().Modern && !Layout().SplitPower) { Queue(Result(api, 4)); break; }
        if (State() != 1) { Queue(Result(api, 3)); break; }
        State(0);
        auto result = Result(api);
        result.End = true;
        Closing = true;
        Queue(std::move(result));
        break;
    }
    case 5:
    case 6:
    {
        if (!Layout().SplitPower) { Queue(Result(api, 4)); break; }
        if (State() != (api == 5 ? 1 : 2)) { Queue(Result(api, 3)); break; }
        if (api == 5)
        {
            Write16(Status + Layout().AllowedChannels, AllowedChannels);
            Write16(Status + Layout().MPChannels, MPChannels(AllowedChannels));
            StoreMAC(Status + Layout().LocalMAC, Mac);
            InitializeLegacyPower();
        }
        State(api == 5 ? 2 : 1);
        Queue(Result(api));
        break;
    }
    case 1:
    case 2:
    {
        if (Layout().LegacyRetail() && api == 2 && State() != 2) { Queue(Result(api, 3)); break; }
        CancelOperations(15);
        for (unsigned id = 0; id < 16; id++) Disconnect(id, true);
        RadioClock.Stop(Console.Timestamp());
        MP = Parent = ScanPending = ConnectPending = false;
        Members = OwnAID = 0;
        ParentID = -1;
        GameInfo.clear();
        ParentBSS.clear();
        Write32(Status + MPOffset(), 0);
        Write16(Status + Layout().Members, 0);
        Write16(Status + Layout().Link, 0);
        Write16(Status + Layout().AID, 0);
        if (!Layout().Firmware) Write16(Status + Layout().Ready, 0);
        if (!Layout().Modern)
        {
            Write16(Status + Layout().EffectiveTX, 0);
            Write16(Status + Layout().EffectiveRX, 0);
        }
        if (Layout().LegacyRetail()) Write16(Status + Layout().Indications, 0);
        State(api == 2 ? 0 : 2);
        auto result = Result(api);
        result.End = api == 2;
        Closing = api == 2;
        Queue(std::move(result));
        NewEpoch();
        break;
    }
    case 7:
    {
        if (State() != 2) { Queue(Result(api, 3)); break; }
        const u32 shift = Layout().Firmware ? 2 : 0;
        const u16 channel = U16(request.Payload.data() + 0x32 - shift);
        const u16 parentSize = U16(request.Payload.data() + 0x34 - shift);
        const u16 childSize = U16(request.Payload.data() + 0x36 - shift);
        const u16 maxEntry = U16(request.Payload.data() + 0x10 - shift);
        if (Layout().LegacyRetail())
        {
            std::copy(request.Payload.begin(), request.Payload.end(), ParentParameters.begin());
            Write(Status + Layout().ParentParameters, ParentParameters.data(), ParentParameters.size());
        }
        if (channel < 1 || channel > 13 || !(AllowedChannels & (1u << channel))
            || parentSize > 512 || childSize > 512 || maxEntry < 1 || maxEntry > 15)
        { Queue(Result(api, 6)); break; }
        std::copy(request.Payload.begin(), request.Payload.end(), ParentParameters.begin());
        Write(Status + Layout().ParentParameters, ParentParameters.data(), ParentParameters.size());
        Channel = channel;
        ParentSize = MaxParentSize = parentSize;
        ChildSize = MaxChildSize = childSize;
        MaxEntry = maxEntry;
        BeaconPeriod = U16(ParentParameters.data() + 0x18 - shift);
        BuildGameInfo(request.Extra);
        TraceGameInfo(request, request.Extra);
        SetSizes();
        Queue(Result(api));
        break;
    }
    case 8:
    {
        if (State() != 2 || GameInfo.empty()) { Queue(Result(api, 3)); break; }
        Parent = true;
        if (Profile == 3) RadioClock.Start(Console.Timestamp());
        ScanSSIDPrefix = 8;
        OwnAID = Members = 0;
        State(7);
        Write16(Status + Layout().OperatingMode, 1);
        Write16(Status + Layout().AID, 0);
        if (!Layout().Modern)
        {
            Write16(Status + Layout().Members, 0);
            Write16(Status + Layout().Ready, 0);
        }
        if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().PowerSave, request.Arg(4) != 0);
            Write16(Status + Layout().Indications, 1);
        }
        SetSizes();
        auto result = Result(api);
        Put16(result.Data.data() + 8, 0);
        Queue(std::move(result));
        NextBeacon = Console.Timestamp() + std::max<u64>(1, BeaconPeriod) * 1024 * 33514 / 1000;
        break;
    }
    case 9:
        if (State() != 7) { Queue(Result(api, 3)); break; }
        for (unsigned id = 0; id < 16; id++)
            Disconnect(id, true, Layout().Firmware ? std::nullopt : std::optional<u16>(3));
        RadioClock.Stop(Console.Timestamp());
        ClearBeaconPhase();
        Parent = false;
        ParentBSS.clear();
        if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().Indications, 0);
            Write16(Status + Layout().Ready, 0);
            for (u16 aid = 0; aid < 16; aid++) LastSeen(aid, 0);
        }
        if (!Layout().Modern)
        {
            Write16(Status + Layout().EffectiveTX, 0);
            Write16(Status + Layout().EffectiveRX, 0);
        }
        State(2);
        Queue(Result(api));
        break;
    case 10:
        if ((State() != 2 && State() != 3 && State() != 5) || ScanPending)
        { Queue(Result(api, 3)); break; }
        if (!Range(request.Arg(4), 192) || request.Half(2) < 1 || request.Half(2) > 13
            || !(AllowedChannels & (1u << request.Half(2)))) { Queue(Result(api, 6)); break; }
        if (State() == 2) ScanSSIDPrefix = 8;
        ScanRequest = request;
        ScanPending = true;
        ScanDeadline = Console.Timestamp() + std::max<u64>(1, request.Half(8)) * 1024 * 33514 / 1000;
        Write32(Status + Layout().ScanBuffer, request.Arg(4));
        Write16(Status + Layout().Channel, request.Half(2));
        if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().OperatingMode, 2);
            Write16(Status + Layout().PowerSave, 1);
        }
        State(5);
        TryScan();
        break;
    case 38:
    {
        if (Profile != 1) { Queue(Result(api, 4)); break; }
        auto result = Result(api);
        Put16(result.Data.data() + 8, 4);
        if (State() != 2 && State() != 3 && State() != 5)
        { Put16(result.Data.data() + 2, 3); Queue(std::move(result)); break; }
        const u16 capacity = request.Half(8), type = request.Half(18);
        const u16 mask = ((u32(request.Half(2)) << 1) & AllowedChannels) >> 1;
        const u16 extent = (u32(capacity) + 1) & ~1u;
        if (!mask || capacity < 64 || capacity > 1024 || (request.Arg(4) & 3)
            || !Range(request.Arg(4), extent) || type > 3 || request.Half(20) > 32
            || (type >= 2 && request.Half(54) > 32) || ((Read32(Status + 0xC8) & 1) && !(type & 1)))
        { Put16(result.Data.data() + 2, 6); Queue(std::move(result)); break; }
        Write32(Status + Layout().ScanBuffer, request.Arg(4));
        Write16(Status + Layout().Channel, request.Half(2));
        Write16(Status + Layout().OperatingMode, 2);
        if (State() == 2)
        {
            ScanSSIDPrefix = 32;
            Write16(Status + Layout().PowerSave, 1);
        }
        if (type >= 2) ScanSSIDPrefix = request.Half(54);
        Write16(Status + 0x1EE, type & 1);
        ScanExConfigured = true;
        State(5);
        const u16 dwell = request.Half(10);
        if (dwell < 10 || dwell > 1000)
        {
            Put16(result.Data.data() + 2, 1);
            Put16(result.Data.data() + 4, 2);
            Put16(result.Data.data() + 6, 5);
            Queue(std::move(result));
            break;
        }
        ScanMask = mask;
        ScanChannel = 1;
        while (!(mask & (1u << (ScanChannel - 1)))) ++ScanChannel;
        ScanInterval = type & 1 ? dwell : std::max<u16>(10, (dwell + 3) / 4);
        ScanElapsed = ScanBytes = ScanCount = 0;
        ScanBudget = (capacity & ~1u) - 2 * ((capacity - 64) / 66);
        ScanData = Bytes{};
        ScanPending = true;
        ScanDeadline = Console.Timestamp() + (TicksPerMillisecond * ScanInterval / 64) * 64;
        TraceScan(TraceStage::ScanWindow, TraceReason::ScanStart);
        ProbeScanEx();
        break;
    }
    case 11:
        if (State() != 5) { Queue(Result(api, 3)); break; }
        if (ScanPending)
        {
            auto result = Result(10);
            Put16(result.Data.data() + 8, 4);
            Queue(std::move(result));
        }
        ScanPending = false;
        if (ScanExConfigured)
        {
            Write16(Status + 0x1EE, 1);
            ScanExConfigured = false;
        }
        State(2);
        Queue(Result(api));
        break;
    case 12:
    {
        if (State() != 2 || ConnectPending) { Queue(Result(api, 3)); break; }
        if (Layout().LegacyRetail()) Write(WM7 + 0x10, request.Payload.data(), request.Payload.size());
        Parent = false;
        ParentID = -1;
        ConnectRequest = request;
        ConnectPending = true;
        ConnectDeadline = Now() + 5000000;
        State(6);
        TryConnect();
        break;
    }
    case 13:
    {
        if (Layout().LegacyRetail() && State() != 7 && State() != 8 && State() != 9 && State() != 10)
        { Queue(Result(api, 3)); break; }
        const u16 wanted = Parent ? request.Half(4) : 1;
        u16 removed = 0;
        for (unsigned id = 0; id < 16; id++)
            if (Peers[id].Connected && (wanted & (Parent ? (1u << Peers[id].AID) : 1)))
            { removed |= Parent ? (1u << Peers[id].AID) : 1; Disconnect(id, true, 0x24); }
        auto result = Result(api);
        Put16(result.Data.data() + 8, wanted);
        Put16(result.Data.data() + 10, removed);
        Queue(std::move(result));
        break;
    }
    case 14:
    {
        if (State() != 7 && State() != 8) { Queue(Result(api, 3)); break; }
        const u32 rx = request.Arg(4), tx = request.Arg(12);
        const u32 rxSize = request.Arg(8), txSize = request.Arg(16);
        if (rxSize < (Layout().LegacyRetail() ? 32u : 64u) || rxSize > 65535 || txSize > 65535 || (rx | tx) & 3
            || !Range(rx, rxSize * 2) || !Range(tx, txSize)) { Queue(Result(api, 6)); break; }
        if (Layout().LegacyRetail())
        {
            if (MP || (!Parent && Read16(Status + Layout().PowerSave) != 1))
            { Queue(Result(api, 3)); break; }
            if (!Parent && (Channel < 1 || Channel > 13 || !(MPChannels(AllowedChannels) & (1u << (Channel - 1)))))
            { Queue(Result(api, 6)); break; }
            const u32 sendMinimum = (Read16(Status + Layout().EffectiveTX) + 31u) & ~31u;
            const u32 receive = Read16(Status + Layout().EffectiveRX);
            const u32 receiveMinimum = ((Parent ? (receive + 12u) * MaxEntry + 10u : receive + 50u) + 31u) & ~31u;
            if ((rxSize | txSize) & 31 || txSize < sendMinimum || rxSize < receiveMinimum)
            { Queue(Result(api, 6)); break; }
        }
        if (Layout().Modern)
        {
            if (!MPParameters[27])
            {
                const u32 sendMinimum = ((Parent ? MaxParentSize + 4u : MaxChildSize + 2u) + 31u) & ~31u;
                const u32 receiveMinimum = (Parent ? (MaxChildSize + 14u) * MaxEntry + 10u + 31u
                    : MaxParentSize + 54u + 31u) & ~31u;
                if (txSize < sendMinimum || rxSize < receiveMinimum) { Queue(Result(api, 6)); break; }
            }
            const u16 error = ApplyParameters(request.Data.data() + 20, nullptr);
            if (error) { Queue(Result(api, error)); break; }
        }
        else
        {
            const u16 frequency = request.Half(20) ? request.Half(20) : 16;
            Write16(Status + Layout().Frequency, frequency);
            Put16(MPParameters.data() + 4, frequency);
            Put16(MPParameters.data() + 6, frequency);
            Put16(MPParameters.data() + 8, frequency);
            Write16(Status + Layout().Retry, request.Half(24));
            Write16(Status + Layout().Booleans, request.Arg(28) != 0);
            Write16(Status + Layout().Booleans + 2, request.Arg(32) != 0);
            Write16(Status + Layout().Booleans + 4, request.Arg(36) != 0);
            Write16(Status + Layout().IgnoreFatal, request.Arg(40) != 0);
        }
        RX = rx; RXSize = rxSize; RXSelector = 0; TX = tx; TXSize = txSize;
        Write16(Status + RXSelectorOffset(), 0);
        Write16(Status + RXSizeOffset(), RXSize);
        Write32(Status + RXOffset(), RX);
        Write32(Status + RXOffset() + 4, RX + RXSize);
        Write32(Status + RXOffset() + 8, TX);
        Write16(Status + RXOffset() + 12, TXSize);
        MP = true;
        ClearRounds();
        MPGeneration = NextMPGeneration++;
        LastVCount = Console.VCount();
        ChildVSync = true;
        Write32(Status + MPOffset(), 1);
        State(Parent ? 9 : 10);
        auto result = Result(api);
        Put16(result.Data.data() + 4, 10);
        if (!Queue(std::move(result))) break;
        for (unsigned id = 0; id < 16; id++)
        {
            if (!Peers[id].Connected) continue;
            std::array<u8, 18> data{};
            Put64(data.data(), Peers[id].Connection);
            Put16(data.data() + 8, 1);
            Put64(data.data() + 10, MPGeneration);
            if (!Send(id, Message::MPState, data.data(), data.size())) Fail("MP state enqueue");
        }
        break;
    }
    case 15:
    {
        Operation operation;
        operation.ID = NextOperation++;
        operation.Started = Console.Timestamp();
        operation.Port = request.Half(16);
        operation.Priority = request.Half(20);
        operation.Size = request.Half(8);
        operation.Retries = Layout().Modern ? U16(MPParameters.data() + 22) : Read16(Status + Layout().Retry);
        operation.Function = request.Arg(24);
        operation.Argument = request.Arg(28);
        operation.DataAddress = request.Arg(4);
        operation.Data = request.Payload;
        operation.RequestedDestinations = Parent ? request.Half(12) : 1;
        operation.Destinations = operation.RequestedDestinations & (Parent ? Members : 1);
        if (!MP) { Complete(operation, Layout().LegacyRetail() ? 3 : 15); break; }
        if (operation.Port >= 16 || operation.Priority >= 4 || operation.Data.empty()
            || operation.Size > (Parent ? ParentSize : ChildSize)) { Complete(operation, 6); break; }
        if (Operations.size() >= MaxOperations) { Complete(operation, 10); break; }
        if (!operation.Destinations) { Complete(operation, 0); break; }
        for (unsigned id = 0; id < 16; id++)
            if (Peers[id].Connected && (operation.Destinations & (Parent ? (1u << Peers[id].AID) : 1)))
            {
                operation.Pending |= 1u << id;
                operation.Connections[id] = Peers[id].Connection;
            }
        if (!operation.Pending) { Complete(operation, 15); break; }
        Operations.push_back(std::move(operation));
        break;
    }
    case 16:
        if (!MP) { Queue(Result(api, 3)); break; }
        CancelOperations(15);
        MP = false;
        Write32(Status + MPOffset(), 0);
        State(Parent ? 7 : 8);
        for (unsigned id = 0; id < 16; id++)
        {
            if (!Peers[id].Connected) continue;
            std::array<u8, 18> data{};
            Put64(data.data(), Peers[id].Connection);
            Put64(data.data() + 10, MPGeneration);
            if (!Send(id, Message::MPState, data.data(), data.size())) Fail("MP state enqueue");
        }
        Queue(Result(api));
        break;
    case 24:
    {
        if (GameInfo.size() < 16) { Queue(Result(api, 3)); break; }
        GameInfo.resize(16 + request.Payload.size());
        const u32 ggid = Layout().Firmware ? request.Half(12) | (u32(request.Half(16)) << 16) : request.Arg(12);
        Put32(GameInfo.data() + 4, ggid);
        Put16(GameInfo.data() + 8, request.Half(Layout().Firmware ? 20 : 16));
        GameInfo[10] = request.Payload.size();
        GameInfo[11] = request.Arg(Layout().Firmware ? 24 : 20);
        std::copy(request.Payload.begin(), request.Payload.end(), GameInfo.begin() + 16);
        if (!Layout().Modern)
        {
            const unsigned shift = Layout().Firmware ? 2 : 0;
            if (Layout().Firmware)
                std::copy_n(Memory(Status + Layout().ParentParameters), ParentParameters.size(), ParentParameters.begin());
            GameInfo[11] &= 0xF;
            Put32(ParentParameters.data(), request.Arg(4));
            Put16(ParentParameters.data() + 4, request.Half(8));
            Put32(ParentParameters.data() + 8 - shift, ggid);
            Put16(ParentParameters.data() + 12 - shift, U16(GameInfo.data() + 8));
            for (unsigned bit = 0; bit < 4; bit++)
                Put16(ParentParameters.data() + (bit == 0 ? 0xE : 0x10 + 2 * bit) - shift, (GameInfo[11] >> bit) & 1);
            Write(Status + Layout().ParentParameters, ParentParameters.data(), ParentParameters.size());
        }
        TraceGameInfo(request, request.Payload);
        Queue(Result(api));
        break;
    }
    case 25:
        if (Layout().LegacyRetail() && request.Arg(4) > 1) { Queue(Result(api, 6)); break; }
        BeaconIndication = request.Arg(4) != 0;
        if (Layout().Modern) Write16(Status + Layout().PowerSave, *BeaconIndication);
        Queue(Result(api));
        break;
    case 29:
    {
        const u16 table = request.Half(4), frameLifeTime = request.Half(12);
        if ((table >= 16 && table != 0xFFFF) || (frameLifeTime > 63 && frameLifeTime != 0xFFFF))
        {
            auto result = Result(api, 1);
            Put16(result.Data.data() + 4, 0x211);
            Put16(result.Data.data() + 6, 5);
            Queue(std::move(result));
            break;
        }
        LifeTimes[3] = 0xFFFF;
        for (u32 i = 0; i < (Layout().Firmware ? 3u : 4u); i++) LifeTimes[i] = request.Half(4 + i * 4);
        LifeTime = LifeTimes[3];
        if (frameLifeTime) FrameLifeTime = frameLifeTime;
        for (unsigned i = 1; i < 16; i++)
            if (table == 0xFFFF || table == i) CAMLifeTimes[i] = LifeTimes[1];
        for (auto& peer : Peers) peer.SeenEmulated = Console.Timestamp();
        for (u16 aid = 0; aid < 16; aid++) LastSeen(aid, (Console.Timestamp() / 64) | 1);
        if (!Layout().Firmware)
        {
            const u64 tick = LifeTime == 0xFFFF ? 0 : LifeTime == 0 ? 1 : (33514ULL * LifeTime * 100) / 64;
            Write32(Status + Layout().Lifetime, tick);
            Write32(Status + Layout().Lifetime + 4, tick >> 32);
        }
        Queue(Result(api));
        break;
    }
    case 30:
    {
        if (Layout().LegacyRetail() && State() != 2) { Queue(Result(api, 3)); break; }
        const u16 channel = request.Half(6);
        if (channel < 1 || channel > 13 || !(AllowedChannels & (1u << channel)))
        { Queue(Result(api, 6)); break; }
        u16 busy = 0;
        for (const auto& peer : Peers)
            if (peer.BSS.size() == 192 && U16(peer.BSS.data() + 0x36) == channel && Now() - peer.Seen < 3000000) busy = 100;
        auto result = Result(api);
        Put16(result.Data.data() + 8, channel);
        Put16(result.Data.data() + 10, busy);
        if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().OperatingMode, 2);
            Write16(Status + Layout().PowerSave, 1);
        }
        State(2);
        Queue(std::move(result));
        break;
    }
    case 33:
    {
        if (!Layout().LegacyRetail()) { Queue(Result(api, 4)); break; }
        if (GameInfo.size() < 16) { Queue(Result(api, 3)); break; }
        Bytes payload;
        std::copy_n(Memory(Status + Layout().ParentParameters), ParentParameters.size(), ParentParameters.begin());
        if (!Copy(payload, U32(ParentParameters.data()), U16(ParentParameters.data() + 4), 112))
        { Queue(Result(api, 6)); break; }
        Put16(ParentParameters.data() + 0xE, request.Half(4));
        Write16(Status + Layout().ParentParameters + 0xE, request.Half(4));
        BuildGameInfo(payload);
        TraceGameInfo(request, payload);
        Queue(Result(api));
        break;
    }
    case 35:
    {
        if (Layout().LegacyRetail())
        {
            const u16 frequency = request.Half(4) ? request.Half(4) : 16;
            Write16(Status + Layout().Frequency, frequency);
            RoundCount = std::min<s32>(static_cast<s16>(Read16(Status + Layout().Frequency + 2)), frequency);
            Write16(Status + Layout().Frequency + 2, RoundCount);
            Queue(Result(api));
            break;
        }
        if (!Layout().Modern) { Queue(Result(api, 4)); break; }
        std::array<u8, 28> old{};
        const u16 error = ApplyParameters(request.Data.data() + 4, &old);
        auto result = Result(api, error);
        Put32(result.Data.data() + 4, request.Arg(4));
        std::copy(old.begin(), old.end(), result.Data.begin() + 8);
        Queue(std::move(result));
        break;
    }
    default:
        Platform::Log(Platform::LogLevel::Warn, "WMHLE unsupported api=%u profile=%u state=%u\n", api, Profile, State());
        Queue(Result(api, 4));
        break;
    }
}

u64 WMService::Impl::OperationLifetime() const
{
    const u64 mp = LifeTime == 0xFFFF ? 0 : LifeTime == 0 ? 64 : (33514ULL * LifeTime * 100 / 64) * 64;
    const u64 frame = FrameLifeTime == 0xFFFF ? 0 : (u64(FrameLifeTime) * BeaconPeriod / 100) * 100 * TicksPerMillisecond;
    return mp && frame ? std::min(mp, frame) : mp ? mp : frame;
}

void WMService::Impl::ClearBeaconPhase()
{
    if (!Parent && BeaconPhaseValid && Status)
    {
        Write32(Status + 0x1C, 0);
        Write32(Status + 0xD4, 0);
    }
    BeaconPhaseValid = ChildAdjustPending = false;
    ConfirmedReplyTick.reset();
    PendingBeacons.clear();
    RepeatedVCount = 0;
}

void WMService::Impl::ClearRounds()
{
    ClearBeaconPhase();
    Round = {};
    Prepared = {};
    Intent = {};
    RoundCount = RoundLimit = 0;
    ChildKick = false;
    ChildPrepareWait = false;
    ChildVSync = true;
    NextPollBitmap = 0xFFFF;
    NextSendTick = NextPrepareTick = 0;
    LastParentRound = 0;
    for (auto& operation : Operations) { operation.Prepared = false; operation.Queued = 0; }
}

Bytes WMService::Impl::RoundHeader(unsigned id, u64 round, u64 preparation, u16 header, u16 flags, u16 size) const
{
    Bytes data(48 + size, 0);
    Put64(data.data(), Peers[id].Connection);
    Put64(data.data() + 8, round);
    Put64(data.data() + 16, preparation);
    Put64(data.data() + 24, MPGeneration);
    Put64(data.data() + 32, Peers[id].MPGeneration);
    Put16(data.data() + 40, Round.PollBitmap);
    Put16(data.data() + 42, header);
    Put16(data.data() + 44, size);
    Put16(data.data() + 46, flags);
    return data;
}

PreparedPacket WMService::Impl::PreparePacket(u16 pollBitmap)
{
    PreparedPacket packet;
    packet.ID = NextPreparation++;
    const bool minPoll = Parent && (Layout().Modern ? MPParameters[24] != 0 : Read16(Status + Layout().Booleans) != 0);
    const bool single = Layout().Modern ? MPParameters[25] != 0 : Read16(Status + Layout().Booleans + 2) != 0;
    const u16 destinationMask = Parent ? (minPoll ? Members : ReadyBitmap) : 1;
    const u16 reachable = Parent ? ReadyBitmap & pollBitmap : 1;
    const size_t capacity = Parent ? ParentSize + 4 : ChildSize + 2;
    u16 blockedPorts = 0;
    for (unsigned priority = 0; priority < 4; priority++)
    {
        for (auto& operation : Operations)
        {
            if (operation.Prepared || operation.Priority != priority || !operation.Pending) continue;
            const u16 portBit = 1u << operation.Port;
            if (blockedPorts & portBit) continue;
            blockedPorts |= portBit;
            const u16 rest = operation.Destinations & ~operation.Sent;
            const u16 destinations = rest & destinationMask;
            if (Parent && !minPoll && (destinations & ~reachable)) continue;
            const bool sequence = operation.Port & 8;
            const bool destination = Parent && ((rest | 1) != 0xFFFF);
            const size_t length = (operation.Data.size() + 1) & ~size_t(1);
            operation.Size = length;
            const size_t needed = length + (sequence ? 2 : 0) + (destination ? 2 : 0)
                + (packet.Operations.empty() ? 0 : 2);
            if (packet.Data.size() + needed > capacity) continue;
            const u16 header = (operation.Port << 8) | ((length / 2) & 255) | (destination ? 0x1000 : 0);
            size_t offset = packet.Data.size();
            packet.Data.resize(offset + needed, 0);
            if (packet.Operations.empty()) packet.Header = header;
            else { Put16(packet.Data.data() + offset, header); offset += 2; }
            std::copy(operation.Data.begin(), operation.Data.end(), packet.Data.begin() + offset);
            offset += length;
            if (sequence)
            {
                if (operation.Sequence == 0xFFFF)
                {
                    operation.Sequence = Sequences[operation.Port & 7];
                    Sequences[operation.Port & 7] = (operation.Sequence + 2) & 0xFFFE;
                }
                Put16(packet.Data.data() + offset, operation.Sequence >> 1);
                offset += 2;
            }
            if (destination) Put16(packet.Data.data() + offset, rest);
            operation.Queued = 0;
            for (unsigned id = 0; id < Peers.size(); id++)
                if ((operation.Pending & (1u << id)) && Peers[id].Connected
                    && operation.Connections[id] == Peers[id].Connection
                    && (destinations & (Parent ? (1u << Peers[id].AID) : 1))) operation.Queued |= 1u << id;
            operation.Prepared = true;
            packet.Operations.push_back(operation.ID);
            packet.Destinations |= operation.Destinations;
            blockedPorts &= ~portBit;
            if (single) return packet;
        }
    }
    return packet;
}

bool WMService::Impl::FlushPacket(const PreparedPacket& packet, u16 failed, bool timeout)
{
    bool retry = false;
    for (u64 operationID : packet.Operations)
    {
        const auto found = std::find_if(Operations.begin(), Operations.end(), [operationID](const Operation& operation) {
            return operation.ID == operationID && operation.Prepared;
        });
        if (found == Operations.end()) continue;
        auto& operation = *found;
        for (unsigned id = 0; id < Peers.size(); id++)
        {
            const u16 peerBit = 1u << id;
            const u16 aidBit = Parent ? (1u << Peers[id].AID) : 1;
            if (!(operation.Queued & peerBit)) continue;
            const bool connected = Peers[id].Connected && operation.Connections[id] == Peers[id].Connection;
            const bool successful = connected && Peers[id].MP && !timeout && !(failed & (Parent ? aidBit : (1u << OwnAID)));
            if (successful) { operation.Sent |= aidBit; operation.Pending &= ~peerBit; }
            else if (!connected) { operation.Failed |= aidBit; operation.Pending &= ~peerBit; }
        }
        operation.Queued = 0;
        operation.Prepared = false;
        const u64 lifetime = OperationLifetime();
        const bool expired = lifetime && Console.Timestamp() - operation.Started >= lifetime;
        if (operation.Pending && !expired && ((operation.Port & 8) || operation.Retries))
        {
            if (operation.Retries) --operation.Retries;
            retry = true;
            continue;
        }
        operation.Failed |= operation.Destinations & ~operation.Sent;
        Complete(operation, operation.Failed ? 15 : 0);
        Operations.erase(found);
    }
    return retry;
}

void WMService::Impl::PortCallbacks(unsigned id, u16 header, const Bytes& data, u32 receiveBase, u32 dataOffset)
{
    std::vector<PortPayload> ports;
    if (!ParsePacket(header, data.data(), data.size(), ports)) { Fail("round port packet"); return; }
    auto& peer = Peers[id];
    for (const auto& port : ports)
    {
        if (port.Port & 8)
        {
            u16& previous = peer.ReceivedSequences[port.Port & 7];
            if (previous != 0xFFFF && u16(previous - port.Sequence) < 256) continue;
            previous = port.Sequence;
        }
        if (!(port.Destinations & (1u << OwnAID))) continue;
        auto result = Result(0x82);
        Put16(result.Data.data() + 4, 21);
        Put16(result.Data.data() + 6, port.Port);
        Put32(result.Data.data() + 8, receiveBase);
        Put32(result.Data.data() + 12, receiveBase + dataOffset + port.Offset);
        Put16(result.Data.data() + 16, port.Length);
        Put16(result.Data.data() + 18, Parent ? peer.AID : 0);
        Put16(result.Data.data() + 26, port.Sequence);
        Put16(result.Data.data() + 32, OwnAID);
        if (Layout().Modern)
        {
            Put16(result.Data.data() + 64, Parent ? ParentSize : ChildSize);
            Put16(result.Data.data() + 66, Parent ? ChildSize : ParentSize);
        }
        Queue(std::move(result));
        ++ReceivedOperations;
    }
}

void WMService::Impl::ReceiveBeaconTiming(unsigned id, const u8* data)
{
    const auto& peer = Peers[id];
    if (Parent || ParentID != static_cast<int>(id) || !TimingLink(peer)
        || U64(data + 206) != peer.Connection || U64(data + 214) != peer.MPGeneration
        || U64(data + 222) != MPGeneration) return;
    const u16 length = U16(data + 202), rate = U16(data + 204);
    if (length != 64 + U16(data + 0x3C) || (rate != 0x14 && rate != 0x0A)) return;
    const u64 transmit = U64(data + 194);
    const u64 endUS = transmit + (length - 4) * (rate == 0x14 ? 4 : 8) - 76;
    const u64 tick = Console.Timestamp();
    if (!BeaconPhaseValid)
    {
        BeaconScheduleOrigin = endUS;
        RadioClock.BaseUS = endUS;
        RadioClock.BaseTick = tick;
        RadioClock.Running = true;
        Write32(Status + 0x1C, 0);
        Write32(Status + 0xD4, 0);
        BeaconPhaseValid = true;
    }
    else if (ConfirmedReplyTick)
    {
        const u64 earliest = BeaconScheduleOrigin + RadioClock.ElapsedUS(*ConfirmedReplyTick);
        const u64 latest = BeaconScheduleOrigin + RadioClock.ElapsedUS(tick);
        if (!RadioReached(transmit, earliest)) BeaconScheduleOrigin += transmit - earliest;
        else if (!RadioReached(latest, transmit)) BeaconScheduleOrigin += transmit - latest;
        ConfirmedReplyTick.reset();
    }
    PendingBeacons.push_back({endUS, BeaconScheduleOrigin, U16(data + 192)});
    ApplyDueBeacons();
}

void WMService::Impl::ApplyDueBeacons()
{
    if (PendingBeacons.empty()) return;
    const u64 elapsed = RadioClock.ElapsedUS(Console.Timestamp());
    while (!PendingBeacons.empty())
    {
        const auto& sample = PendingBeacons.front();
        if (!RadioReached(sample.OriginUS + elapsed, sample.EndUS)) break;
        RadioClock.BaseUS = sample.OriginUS;
        BeaconVTSF = sample.VTSF;
        Write16(0x0380FFF0, BeaconVTSF);
        PendingBeacons.pop_front();
    }
}

void WMService::Impl::UpdateBeaconPhase(u16 previous, u16 current, bool wrapped)
{
    const auto crossed = [&](u16 target) {
        return wrapped ? previous < target || current >= target : previous < target && current >= target;
    };
    if (Parent)
    {
        if (!crossed(209) || !RadioClock.Running
            || std::none_of(Peers.begin(), Peers.end(), [&](const auto& peer) { return TimingLink(peer); })) return;
        const u16 count = Read16(Status + 0xD8);
        if (count < 60) Write16(Status + 0xD8, count + 1);
        else if (current >= 209 && current < 212)
        {
            Write16(0x04000006, current);
            Write16(Status + 0xD8, 0);
        }
        BeaconVTSF = u16((2 * u32(RadioClock.At(Console.Timestamp())) - 127 * current) >> 7);
        Write16(0x0380FFF0, BeaconVTSF);
        BeaconPhaseValid = true;
        return;
    }
    if (!BeaconPhaseValid) return;
    if (crossed(200))
    {
        Write32(Status + 0xCC, BeaconVTSF);
        if (Read32(Status + 0xD0) != BeaconVTSF)
        {
            Write32(Status + 0xD0, BeaconVTSF);
            constexpr u32 mask = 0x3FFFC0;
            constexpr u32 frameUS = 16715;
            const u32 tsf = u32(RadioClock.At(Console.Timestamp())) & mask;
            const u32 projected = (tsf + (263u - current) * 127 / 2) & mask;
            u32 cursor = u32(BeaconVTSF) << 6;
            u32 residual = 0;
            if (cursor <= projected)
            {
                const u32 frames = (projected - cursor) / frameUS + 1;
                cursor += std::min(frames, 29u) * frameUS;
                if (frames <= 29 && cursor - projected <= 0x400E) residual = cursor - projected;
            }
            Write32(Status + 0xCC, cursor);
            Write32(Status + 0xD4, residual);
        }
        ChildAdjustPending = Read32(Status + 0xD4) > 127;
        if (!ChildAdjustPending) Write32(Status + 0x1C, 1);
    }
    if (ChildAdjustPending && crossed(208))
    {
        ChildAdjustPending = false;
        u32 residual = Read32(Status + 0xD4);
        if (current >= 208 && current < 212 && residual >= 127)
        {
            const u32 units = std::min(7u, 1 + (residual - 127) / 63);
            RepeatedVCount = current + 1 - units;
            RepeatedVCountFrame = *Console.Host.frame;
            Write16(0x04000006, RepeatedVCount);
            residual -= units * 63;
            Write32(Status + 0xD4, residual);
        }
        if (residual >= 127) Write32(Status + 0x1C, 0);
    }
}

void WMService::Impl::UpdateRoundBudget()
{
    ApplyDueBeacons();
    const u16 current = Console.VCount();
    const u16 target = Layout().Modern ? U16(MPParameters.data() + (Parent ? 18 : 20)) : Parent ? 260 : LegacyChildMPLine;
    const bool repeated = RepeatedVCount && *Console.Host.frame == RepeatedVCountFrame && current >= RepeatedVCount;
    const bool wrapped = current < LastVCount && !repeated;
    if (current != LastVCount) RepeatedVCount = 0;
    const bool crossed = wrapped ? LastVCount < target || current >= target : LastVCount < target && current >= target;
    if (Profile == 3 && current != LastVCount) UpdateBeaconPhase(LastVCount, current, wrapped);
    LastVCount = current;
    if (!crossed) return;
    if (!Parent)
    {
        if (!Prepared.ID && !Round.Packet.ID && (!Layout().Firmware || ChildVSync))
        { ChildPrepareWait = ChildVSync; ChildVSync = false; ChildKick = true; }
        return;
    }
    if (!Members) { RoundCount = 0; return; }
    const u16 frequency = Layout().Modern ? U16(MPParameters.data() + 6) : Read16(Status + Layout().Frequency);
    if (Layout().Modern && RoundCount < 0) RoundCount = 0;
    if (Layout().Firmware) RoundCount = std::min<u32>(u16(RoundCount + frequency), 256);
    else RoundCount = std::min<s32>(static_cast<s16>(RoundCount + frequency), 256);
    RoundLimit = Layout().Modern ? static_cast<s16>(U16(MPParameters.data() + 8))
        : Layout().Firmware ? 256 : Read16(Status + Layout().Booleans + 4) ? frequency : 6;
    Write16(Status + (Layout().Modern ? 0x62 : Layout().Frequency + 2), RoundCount);
    if (!Layout().Firmware) Write16(Status + (Layout().Modern ? 0x64 : 0x38), RoundLimit);
}

void WMService::Impl::StartRound()
{
    MPRound next;
    next.Started = Console.Timestamp();
    u16 eligible = 0;
    for (unsigned id = 0; id < Peers.size(); id++)
    {
        const auto& peer = Peers[id];
        if (!peer.Connected || !peer.MP || !Negotiated(id)) continue;
        eligible |= 1u << peer.AID;
        auto& response = next.Responses[id];
        response.Epoch = peer.Epoch;
        response.Connection = peer.Connection;
        response.MPGeneration = peer.MPGeneration;
        response.AID = peer.AID;
    }
    const u16 retryBitmap = NextPollBitmap & Members;
    if (retryBitmap) eligible &= retryBitmap;
    if (!eligible) return;
    next.ID = NextRound++;
    next.Packet = PreparePacket(eligible);
    const bool minPoll = Layout().Modern ? MPParameters[24] != 0 : Read16(Status + Layout().Booleans) != 0;
    next.PollBitmap = minPoll && next.Packet.Destinations ? eligible & next.Packet.Destinations : eligible;
    next.VSync = RoundCount == 1 || (!Layout().Firmware && RoundLimit == 1);
    next.WaitUntil = Now() + 25000;
    if (next.VSync) next.Packet.Header |= 0x8000;
    for (unsigned id = 0; id < Peers.size(); id++)
        if (next.Responses[id].Connection && (next.PollBitmap & (1u << next.Responses[id].AID))) next.Expected |= 1u << id;
    next.Remaining = next.Expected;
    Round = std::move(next);
    TrySendRound();
}

void WMService::Impl::TrySendRound()
{
    if (!Parent || !MP || Failed || !Round.ID || Round.Closing || Round.Terminating) return;
    const u64 lifetime = OperationLifetime();
    if (lifetime && Console.Timestamp() - Round.Started >= lifetime) { FinishRound(); return; }
    for (unsigned id = 0; id < Peers.size(); id++)
    {
        auto& response = Round.Responses[id];
        const auto& peer = Peers[id];
        if (!response.Connection || response.CancelSent) continue;
        if (!peer.Connected || !peer.MP || response.Epoch != peer.Epoch || response.Connection != peer.Connection
            || response.MPGeneration != peer.MPGeneration)
        {
            Round.Remaining &= ~(1u << id);
            if (Round.Expected & (1u << id)) Round.Failed |= 1u << response.AID;
            response.Connection = 0;
            continue;
        }
        const bool fallback = response.IntentSent && (Round.Remaining & (1u << id))
            && !response.FallbackSent && Now() >= Round.WaitUntil;
        if (response.IntentSent && !fallback) continue;
        auto packet = RoundHeader(id, Round.ID, Round.Packet.ID, Round.Packet.Header, 1, Round.Packet.Data.size());
        std::copy(Round.Packet.Data.begin(), Round.Packet.Data.end(), packet.begin() + 48);
        if (!Send(id, fallback ? Message::RoundPoll : Message::RoundIntent, packet.data(), packet.size()))
        {
            Round.Remaining &= ~(1u << id);
            Round.Failed |= 1u << response.AID;
            if (!CancelRoundPeer(id)) return;
            if (!response.IntentSent) response.Connection = 0;
        }
        else if (fallback) response.FallbackSent = true;
        else response.IntentSent = true;
    }
    if (!Round.Remaining) FinishRound();
}

bool WMService::Impl::CancelRoundPeer(unsigned id)
{
    auto& response = Round.Responses[id];
    if (!response.IntentSent || response.Received || response.CancelSent) return true;
    const auto& peer = Peers[id];
    if (!peer.Connected || !peer.MP || peer.Epoch != response.Epoch || peer.Connection != response.Connection
        || peer.MPGeneration != response.MPGeneration) return true;
    auto packet = RoundHeader(id, Round.ID, Round.Packet.ID, 0, 0, 0);
    if (!Send(id, Message::RoundCancel, packet.data(), 40)) { Fail("round cancel enqueue"); return false; }
    response.CancelSent = true;
    return true;
}

void WMService::Impl::ReceiveIntent(unsigned id, const u8* data, u16 size, bool fallback)
{
    const auto& peer = Peers[id];
    if (Parent || !MP || Failed || ParentID != static_cast<int>(id) || !peer.Connected || !peer.MP || size < 48
        || U64(data) != peer.Connection || U64(data + 24) != peer.MPGeneration || U64(data + 32) != MPGeneration
        || !U64(data + 8) || U16(data + 46) > 1 || size != 48 + U16(data + 44)
        || U16(data + 44) > ParentSize + 4) return;
    const bool prepared = U16(data + 46) != 0;
    if (prepared != (U64(data + 16) != 0) || (!prepared && (size != 48 || U16(data + 42)))) return;
    std::vector<PortPayload> parsed;
    if (!ParsePacket(U16(data + 42), data + 48, size - 48, parsed)) return;
    if (!Intent.Data.empty() && Intent.LocalEpoch == Epoch && Intent.RemoteEpoch == peer.Epoch && Intent.PeerID == id)
    {
        if (U64(Intent.Data.data() + 8) == U64(data + 8))
        {
            if (Intent.Data.size() != size || !std::equal(Intent.Data.begin(), Intent.Data.end(), data))
            { Fail("round intent conflict"); return; }
            if (Intent.State == RoundIntent::Stage::Pending) PromoteIntent(fallback);
            return;
        }
        if (U64(data + 8) <= LastParentRound) return;
        if (Intent.State != RoundIntent::Stage::Retired) { Fail("round intent occupied"); return; }
    }
    if (U64(data + 8) <= LastParentRound) return;
    Intent = {};
    Intent.PeerID = id;
    Intent.LocalEpoch = Epoch;
    Intent.RemoteEpoch = peer.Epoch;
    Intent.Data.assign(data, data + size);
    PromoteIntent(fallback);
}

void WMService::Impl::PromoteIntent(bool fallback)
{
    if (Intent.Data.empty() || Intent.State != RoundIntent::Stage::Pending || !MP || Parent || Failed) return;
    const unsigned id = Intent.PeerID;
    const auto& peer = Peers[id];
    if (ParentID != static_cast<int>(id) || Intent.LocalEpoch != Epoch || Intent.RemoteEpoch != peer.Epoch
        || !peer.Connected || !peer.MP || U64(Intent.Data.data()) != peer.Connection
        || U64(Intent.Data.data() + 24) != peer.MPGeneration || U64(Intent.Data.data() + 32) != MPGeneration)
    { Intent = {}; return; }
    if (!fallback && (U16(Intent.Data.data() + 40) & (1u << OwnAID)) && !Prepared.ID) return;
    auto owned = std::move(Intent);
    Intent = {};
    ReceivePoll(id, owned.Data.data(), owned.Data.size());
    if (!Failed && MP && !Parent && ParentID == static_cast<int>(id) && Epoch == owned.LocalEpoch
        && Peers[id].Epoch == owned.RemoteEpoch && Peers[id].Connection == U64(owned.Data.data())
        && Peers[id].MPGeneration == U64(owned.Data.data() + 24) && MPGeneration == U64(owned.Data.data() + 32)
        && Round.ID == U64(owned.Data.data() + 8))
    {
        owned.State = RoundIntent::Stage::Promoted;
        Intent = std::move(owned);
    }
}

void WMService::Impl::ReceiveCancel(unsigned id, const u8* data, u16 size)
{
    if (Parent || !MP || Failed || size != 40 || Intent.Data.empty() || Intent.PeerID != id
        || Intent.LocalEpoch != Epoch || Intent.RemoteEpoch != Peers[id].Epoch
        || !std::equal(data, data + 40, Intent.Data.begin())) return;
    LastParentRound = std::max(LastParentRound, U64(data + 8));
    if (Intent.State == RoundIntent::Stage::Promoted && Round.ID == U64(data + 8))
        FinishChild(1u << OwnAID, true);
    Intent.State = RoundIntent::Stage::Retired;
}

void WMService::Impl::ReceivePoll(unsigned id, const u8* data, u16 size)
{
    const auto& peer = Peers[id];
    if (Parent || !MP || ParentID != static_cast<int>(id) || !peer.Connected || !peer.MP || size < 48
        || U64(data) != peer.Connection || U64(data + 24) != peer.MPGeneration || U64(data + 32) != MPGeneration
        || U64(data + 8) <= LastParentRound || U16(data + 46) > 1
        || size != 48 + U16(data + 44) || U16(data + 44) > ParentSize + 4) return;
    const bool prepared = U16(data + 46) != 0;
    if (prepared != (U64(data + 16) != 0) || (!prepared && (size != 48 || U16(data + 42)))) return;
    std::vector<PortPayload> parsed;
    if (!ParsePacket(U16(data + 42), data + 48, size - 48, parsed)) return;
    if (Round.ID) FinishChild(1u << OwnAID, true);
    Round = {};
    Round.ID = LastParentRound = U64(data + 8);
    Round.Started = Console.Timestamp();
    Round.PollBitmap = U16(data + 40);
    Round.Polled = Round.PollBitmap & (1u << OwnAID);
    Round.VSync = U16(data + 42) & 0x8000;
    auto& response = Round.Responses[id];
    response.Epoch = peer.Epoch;
    response.Connection = peer.Connection;
    response.MPGeneration = peer.MPGeneration;
    if (Round.Polled)
    {
        Round.Packet = std::move(Prepared);
        Prepared = {};
        auto packet = RoundHeader(id, Round.ID, Round.Packet.ID, Round.Packet.Header, Round.Packet.ID ? 1 : 0, Round.Packet.Data.size());
        std::copy(Round.Packet.Data.begin(), Round.Packet.Data.end(), packet.begin() + 48);
        if (!Send(id, Message::RoundReply, packet.data(), packet.size())) Fail("round reply enqueue");
    }
    if (!Round.Polled && !Layout().Firmware) return;
    RXSelector ^= 1;
    Write16(Status + RXSelectorOffset(), RXSelector);
    const u32 receiveBase = RX + RXSelector * RXSize;
    const u16 length = size - 48;
    const size_t extent = (prepared ? 50 : 48) + length;
    if (extent > RXSize || Callbacks.size() + parsed.size() + 2 >= MaxCallbacks)
    { Fail("round child receive capacity"); return; }
    Bytes receive(Memory(receiveBase), Memory(receiveBase) + extent);
    Put16(receive.data() + 6, length);
    const u16 timestamp = Console.Timestamp() / 536;
    Put16(receive.data() + 10, timestamp);
    Put16(receive.data() + 12, timestamp);
    Put16(receive.data() + 14, 0xFF14);
    std::copy(Mac.begin(), Mac.end(), receive.begin() + 24);
    std::copy(peer.Mac.begin(), peer.Mac.end(), receive.begin() + 30);
    if (Layout().Firmware) EncodeMAC(receive.data() + 24, peer.Mac);
    Put16(receive.data() + 42, static_cast<u16>(Round.ID << 4));
    Round.ReplySlotTime = (ChildSize + 32) * 4 + 102;
    Put16(receive.data() + 44, Round.ReplySlotTime);
    Put16(receive.data() + 46, Round.PollBitmap);
    if (prepared)
    {
        Put16(receive.data() + 48, U16(data + 42));
        std::copy(data + 48, data + size, receive.begin() + 50);
    }
    auto result = Result(14, !Round.Polled ? 13 : prepared || Layout().Modern ? 0 : 14);
    Put16(result.Data.data() + 4, 12);
    Put32(result.Data.data() + 8, receiveBase);
    result.ReceiveBase = receiveBase;
    result.Receive = std::move(receive);
    Queue(std::move(result));
    PortCallbacks(id, U16(data + 42), Bytes(data + 48, data + size), receiveBase, 50);
}

void WMService::Impl::ReceiveReply(unsigned id, const u8* data, u16 size)
{
    const u64 lifetime = OperationLifetime();
    if (Parent && MP && Round.ID && !Round.Closing && lifetime && Console.Timestamp() - Round.Started >= lifetime)
    { FinishRound(); return; }
    auto& peer = Peers[id];
    if (!Parent || !MP || !Round.ID || !Round.Responses[id].IntentSent || Round.Terminating || Round.Closing || !(Round.Remaining & (1u << id)) || size < 48
        || U64(data) != Round.Responses[id].Connection || U64(data + 8) != Round.ID
        || peer.Epoch != Round.Responses[id].Epoch || U64(data + 24) != Round.Responses[id].MPGeneration
        || U64(data + 32) != MPGeneration || U16(data + 40) != Round.PollBitmap
        || U16(data + 46) > 1 || size != 48 + U16(data + 44) || U16(data + 44) > ChildSize + 2) return;
    const bool prepared = U16(data + 46) != 0;
    if (prepared != (U64(data + 16) != 0) || (!prepared && (U16(data + 42) || size != 48))) return;
    std::vector<PortPayload> parsed;
    if (!ParsePacket(U16(data + 42), data + 48, size - 48, parsed)) return;
    auto& response = Round.Responses[id];
    response.Received = true;
    response.Prepared = prepared;
    response.Preparation = U64(data + 16);
    response.Header = U16(data + 42);
    response.Data.assign(data + 48, data + size);
    Round.Remaining &= ~(1u << id);
    if (!Round.Remaining) FinishRound();
}

void WMService::Impl::FinishRound()
{
    if (!Round.ID || Round.Closing || Failed) return;
    Round.Terminating = true;
    for (unsigned id = 0; id < Peers.size(); id++)
        if ((Round.Expected & (1u << id)) && !Round.Responses[id].Received && !CancelRoundPeer(id)) return;
    unsigned count = 0;
    size_t needed = Round.Packet.Operations.size() + 1;
    for (unsigned id = 0; id < Peers.size(); id++)
    {
        if (!(Round.Expected & (1u << id))) continue;
        ++count;
        if (!Round.Responses[id].Received) Round.Failed |= 1u << Round.Responses[id].AID;
        std::vector<PortPayload> ports;
        const auto& response = Round.Responses[id];
        if (response.Prepared && ParsePacket(response.Header, response.Data.data(), response.Data.size(), ports))
            needed += ports.size();
    }
    const u16 stride = (ChildSize + 15) & ~1u;
    const size_t extent = 10 + count * stride;
    if (extent > RXSize)
    { Fail("round parent receive capacity"); return; }
    if (Callbacks.size() + needed > MaxCallbacks) return;
    Round.Closing = true;
    const u32 receiveBase = RX + RXSelector * RXSize;
    Bytes receive(Memory(receiveBase), Memory(receiveBase) + extent);
    Put16(receive.data(), Round.Failed);
    Put16(receive.data() + 2, 0);
    Put16(receive.data() + 4, count);
    Put16(receive.data() + 6, stride);
    Put16(receive.data() + 8, 1);
    const size_t firstCallback = Callbacks.size();
    size_t offset = 10;
    u16 flushFailed = Round.Failed;
    for (unsigned id = 0; id < Peers.size(); id++)
    {
        const auto& response = Round.Responses[id];
        if (Round.Expected & (1u << id))
        {
            Put16(receive.data() + offset, response.Received ? response.Data.size() : 0xFFFF);
            Put16(receive.data() + offset + 2, response.Received ? 0xFF14 : 0);
            Put16(receive.data() + offset + 4, response.AID);
            Put16(receive.data() + offset + 6, response.Received ? 0 : 1);
            Put16(receive.data() + offset + 8, response.Header);
            if (response.Prepared)
            {
                ReadyBitmap |= 1u << response.AID;
                std::copy(response.Data.begin(), response.Data.end(), receive.begin() + offset + 10);
                PortCallbacks(id, response.Header, response.Data, receiveBase, offset + 10);
            }
            else if (response.Received && !Layout().Firmware) flushFailed |= 1u << response.AID;
            offset += stride;
        }
        const auto& peer = Peers[id];
        if (!response.IntentSent || response.CancelSent || ((Round.Expected & (1u << id)) && !response.Received)
            || !response.Connection || response.Epoch != peer.Epoch
            || response.Connection != peer.Connection || response.MPGeneration != peer.MPGeneration) continue;
        if (Layout().Firmware && response.Prepared && peer.Connected && peer.MP)
            Write16(Status + Layout().Ready, Read16(Status + Layout().Ready) | (1u << response.AID));
        auto packet = RoundHeader(id, Round.ID, response.Preparation, 0, 0, 22);
        Put16(packet.data() + 40, Round.Failed);
        Put16(packet.data() + 48, Console.Timestamp() / 536);
        Put16(packet.data() + 50, 0xFF14);
        std::copy(Peers[id].Mac.begin(), Peers[id].Mac.end(), packet.begin() + 52);
        std::copy(Mac.begin(), Mac.end(), packet.begin() + 58);
        Put16(packet.data() + 64, static_cast<u16>(Round.ID << 4));
        const u16 target = Layout().Modern ? U16(MPParameters.data() + 18) : 260;
        Put16(packet.data() + 66, ((target + 263 - Console.VCount()) % 263) * 2130 / 536);
        Put16(packet.data() + 68, Round.Failed);
        if (!Send(id, Message::RoundAck, packet.data(), packet.size())) Fail("round ACK enqueue");
    }
    if (!Layout().Firmware) Write16(Status + Layout().Ready, ReadyBitmap);
    FlushPacket(Round.Packet, flushFailed, false);
    auto result = Result(14);
    Put16(result.Data.data() + 4, 11);
    Put32(result.Data.data() + 8, receiveBase);
    result.RoundID = Round.ID;
    result.RoundEpoch = Epoch;
    result.RoundOwner = MPGeneration;
    result.RoundEvent = 1;
    Queue(std::move(result));
    if (firstCallback < Callbacks.size())
    {
        Callbacks[firstCallback].ReceiveBase = receiveBase;
        Callbacks[firstCallback].Receive = std::move(receive);
    }
}

void WMService::Impl::ReleaseRound(u64 id)
{
    if (!Parent || !Round.Closing || Round.ID != id) return;
    const u16 retryBitmap = Round.Failed & Members;
    const bool retry = retryBitmap && (!Layout().Firmware || (!Read16(Status + Layout().Booleans + 4)
        && Read16(Status + Layout().Frequency)));
    if (!retry) --RoundCount;
    if (!Layout().Firmware && RoundLimit > 0) --RoundLimit;
    NextPollBitmap = retry ? retryBitmap : 0xFFFF;
    Write16(Status + (Layout().Modern ? 0x62 : Layout().Frequency + 2), RoundCount);
    if (!Layout().Firmware) Write16(Status + (Layout().Modern ? 0x64 : 0x38), RoundLimit);
    RXSelector ^= 1;
    Write16(Status + RXSelectorOffset(), RXSelector);
    Round = {};
    const u16 interval = Layout().Modern ? U16(MPParameters.data() + 14) : 0;
    NextSendTick = Console.Timestamp() + u64(interval) * 33514 / 1000;
}

void WMService::Impl::FinishChild(u16 failed, bool timeout, const u8* metadata)
{
    if (!Round.ID) return;
    const u64 id = Round.ID;
    if (!Intent.Data.empty() && U64(Intent.Data.data() + 8) == id) Intent.State = RoundIntent::Stage::Retired;
    const bool polled = Round.Polled;
    const bool retry = Round.Packet.ID && FlushPacket(Round.Packet, failed, timeout);
    ChildVSync = Round.VSync && !retry;
    NextPrepareTick = Console.Timestamp() + (Layout().Modern ? u64(U16(MPParameters.data() + 16)) * 33514 / 1000 : 0);
    if (polled || Layout().Firmware)
    {
        const u16 error = !polled ? 13 : timeout ? 9 : (failed & (1u << OwnAID)) ? 15 : 0;
        auto result = Result(14, error);
        Put16(result.Data.data() + 4, 13);
        if (!timeout && metadata)
        {
            result.AckMetadata = true;
            std::copy_n(metadata, 22, result.Data.begin() + 12);
            if (Layout().Firmware)
            {
                const MAC destination = ReadMAC(metadata + 4), source = ReadMAC(metadata + 10);
                EncodeMAC(result.Data.data() + 16, destination);
                EncodeMAC(result.Data.data() + 22, source);
            }
        }
        result.RoundID = id;
        result.RoundEpoch = Epoch;
        result.RoundOwner = MPGeneration;
        result.RoundEvent = polled && !ChildVSync ? 2 : 0;
        Queue(std::move(result));
    }
    Round = {};
}

void WMService::Impl::ReceiveRoundAck(unsigned id, const u8* data, u16 size)
{
    if (Parent || !MP || ParentID != static_cast<int>(id) || !Round.ID || size != 70
        || U64(data) != Round.Responses[id].Connection || U64(data + 8) != Round.ID
        || (U64(data + 16) != Round.Packet.ID && (U64(data + 16) || !(U16(data + 40) & (1u << OwnAID))))
        || Peers[id].Epoch != Round.Responses[id].Epoch
        || U64(data + 24) != Round.Responses[id].MPGeneration || U64(data + 32) != MPGeneration
        || U16(data + 42) || U16(data + 44) != 22 || U16(data + 46) || U16(data + 40) != U16(data + 68)) return;
    if (BeaconPhaseValid && Round.Polled && !(U16(data + 40) & (1u << OwnAID))
        && Round.Started >= RadioClock.BaseTick)
        ConfirmedReplyTick = Round.Started;
    FinishChild(U16(data + 40), false, data + 48);
}

void WMService::Impl::Network(const WMServicePacket& packet)
{
    const u64 cause = TracePacket(TraceStage::Receive, TraceReason::None, packet);
    const auto trace = [&](TraceReason reason) { TracePacket(TraceStage::NetworkDecision, reason, packet, cause); };
    if (packet.SenderID >= 16) { trace(TraceReason::PeerID); return; }
    if (packet.SenderID == LocalID) { trace(TraceReason::Self); return; }
    if (packet.Length < 24) { trace(TraceReason::HeaderShort); return; }
    if (!(Present & (1u << packet.SenderID))) { trace(TraceReason::Absent); return; }
    const u8* header = packet.Data.data();
    if (U32(header) != Magic) { trace(TraceReason::Magic); return; }
    if (U16(header + 4) != WMServiceVersion) { trace(TraceReason::Version); return; }
    if (header[7]) { trace(TraceReason::Reserved); return; }
    const auto type = static_cast<Message>(header[6]);
    const u64 senderEpoch = U64(header + 8), receiverEpoch = U64(header + 16);
    if (!senderEpoch) { trace(TraceReason::SenderEpochZero); return; }
    const unsigned id = packet.SenderID;
    auto& peer = Peers[id];
    const u8* data = header + 24;
    const u16 size = packet.Length - 24;
    if (type == Message::Offer)
    {
        if (receiverEpoch) { trace(TraceReason::OfferReceiver); return; }
        if (size != 8) { trace(TraceReason::OfferSize); return; }
        if (data[1]) { trace(TraceReason::OfferReserved); return; }
        if (data[0] < 1 || data[0] > 4)
        {
            trace(TraceReason::OfferProfile);
            Fatal("unsupported peer WM owner");
            return;
        }
        const bool changed = peer.Epoch != senderEpoch;
        if (!changed && (peer.Profile != data[0] || !std::equal(peer.Mac.begin(), peer.Mac.end(), data + 2)))
        {
            trace(TraceReason::OfferIdentity);
            Fatal("peer identity changed inside epoch");
            return;
        }
        if (changed)
        {
            if (Initialized) Disconnect(id, false);
            peer = {};
            peer.Epoch = senderEpoch;
            peer.Profile = data[0];
            std::copy(data + 2, data + 8, peer.Mac.begin());
        }
        peer.Seen = Now();
        peer.SeenEmulated = Console.Timestamp();
        if (Mode == Backend::LLE) { trace(TraceReason::OfferLLE); Send(id, Message::Reject); return; }
        if (Mode == Backend::Undecided) { trace(TraceReason::OfferUndecided); return; }
        if (Profile == 0) { trace(TraceReason::OfferNoProfile); return; }
        if (changed) Offer(id);
        Send(id, Message::Ready);
        trace(TraceReason::OfferAccepted);
        return;
    }
    if (type == Message::Reject && !size && receiverEpoch == Epoch)
    {
        trace(TraceReason::Rejected);
        Reject();
        return;
    }
    if (receiverEpoch != Epoch)
    {
        trace(TraceReason::ReceiverEpoch);
        ++RejectedEpochs;
        return;
    }
    if (peer.Epoch != senderEpoch)
    {
        trace(TraceReason::PeerEpoch);
        ++RejectedEpochs;
        return;
    }
    peer.Seen = Now();
    peer.SeenEmulated = Console.Timestamp();
    if (peer.Connected) LastSeen(Parent ? peer.AID : 0, (peer.SeenEmulated / 64) | 1);
    if (type == Message::Reject) { trace(TraceReason::Rejected); if (!size) Reject(); return; }
    if (type == Message::Ready)
    {
        if (size) { trace(TraceReason::ReadySize); return; }
        if (Mode == Backend::LLE) { trace(TraceReason::ReadyLLE); return; }
        if (Mode == Backend::Undecided) { trace(TraceReason::ReadyUndecided); return; }
        peer.Ready = true;
        SendTimingSupport(id);
        trace(TraceReason::ReadyAccepted);
        return;
    }
    if (Mode != Backend::HLE) { trace(TraceReason::NotHLE); return; }
    if (type == Message::BeaconTimingSupport)
    {
        if (!size && Profile == 3 && peer.Profile == 3 && !Failed) peer.TimingSupportReceived = true;
        return;
    }
    if (!Initialized) { trace(TraceReason::NotInitialized); return; }
    if (!peer.Ready) { trace(TraceReason::PeerNotReady); return; }
    if (Failed) { trace(TraceReason::Failed); return; }
    switch (type)
    {
    case Message::Advertisement:
        if (size != 192 && !(size == 230 && Profile == 3 && peer.Profile == 3
            && peer.TimingSupportReceived && peer.TimingSupportSent)) { trace(TraceReason::AdvertisementSize); return; }
        if (U16(data + 0x3C) < 16 || U16(data + 0x3C) > 128) { trace(TraceReason::GameLength); return; }
        if (U16(data + 0x36) < 1 || U16(data + 0x36) > 13) { trace(TraceReason::Channel); return; }
        if (!std::equal(peer.Mac.begin(), peer.Mac.end(), data + 4)) { trace(TraceReason::PeerMAC); return; }
        peer.BSS.assign(data, data + 192);
        if (size == 230) ReceiveBeaconTiming(id, data);
        trace(TraceReason::AdvertisementAccepted);
        TryScan(id, cause);
        if (peer.Connected && !Parent && BeaconIndication.value_or(false)
            && (!Layout().LegacyRetail() || (Read16(Status + Layout().Indications)
                && (State() == 8 || State() == 10) && U16(data + 0x48) == Read16(Status + Layout().ConnectedTGID))))
        {
            auto result = Result(0x80);
            Put16(result.Data.data() + 4, 16);
            Put16(result.Data.data() + 6, U16(data + 0x48));
            Put16(result.Data.data() + 8, State());
            const u16 length = U16(data + 0x3C);
            Put16(result.Data.data() + 10, length);
            std::copy(data + 0x40, data + 0x40 + length, result.Data.begin() + 12);
            Queue(std::move(result));
        }
        break;
    case Message::ScanProbe:
    {
        if (size != 52) { trace(TraceReason::ProbeSize); break; }
        if (!U64(data)) { trace(TraceReason::ProbeToken); break; }
        if (U16(data + 18)) { trace(TraceReason::ProbeReserved); break; }
        if (!Parent) { trace(TraceReason::NotParent); break; }
        if (State() != 7 && State() != 9) { trace(TraceReason::ParentState); break; }
        if (GameInfo.size() < 16) { trace(TraceReason::NoGameInfo); break; }
        if (U16(data + 8) != Channel) { trace(TraceReason::ProbeChannel); break; }
        if (U16(data + 10) != 32) { trace(TraceReason::ProbeSSIDLength); break; }
        const MAC filter = ReadMAC(data + 12);
        const bool wildcard = std::all_of(filter.begin(), filter.end(), [](u8 value) { return value == 0xFF; });
        if (!wildcard && filter != Mac) { trace(TraceReason::ProbeMAC); break; }
        std::array<u8, 200> response{};
        BuildBSS(response.data() + 8);
        if (!std::equal(response.begin() + 20, response.begin() + 28, data + 20)) { trace(TraceReason::ProbeSSID); break; }
        Put64(response.data(), U64(data));
        if (!Send(id, Message::ScanResponse, response.data(), response.size())) Fail("scan response enqueue");
        trace(TraceReason::ProbeAccepted);
        break;
    }
    case Message::ScanResponse:
        if (size != 200) { trace(TraceReason::ResponseSize); break; }
        if (!ScanPending) { trace(TraceReason::NoScan); break; }
        if (ScanRequest.API() != 38) { trace(TraceReason::ScanAPI); break; }
        if (ScanRequest.Half(18) & 1) { trace(TraceReason::PassiveScan); break; }
        if (U64(data) != ScanToken) { trace(TraceReason::ResponseToken); break; }
        trace(TraceReason::ResponseAccepted);
        ObserveScanEx(id, data + 8, 192, cause);
        break;
    case Message::Connect:
    {
        if (size != 44 || !U64(data) || !std::equal(peer.Mac.begin(), peer.Mac.end(), data + 8)) return;
        const u64 connection = U64(data);
        u16 error = 0, aid = 0;
        if (!Parent || (State() != 7 && State() != 9) || GameInfo.size() < 16) error = 3;
        else if (!(GameInfo[11] & 1)) error = 11;
        else if (!std::equal(GameInfo.begin() + 4, GameInfo.begin() + 10, data + 14)) error = 6;
        else
        {
            if (peer.Connected && peer.Connection == connection) aid = peer.AID;
            else
            {
                for (u16 candidate = 1; candidate <= MaxEntry; candidate++)
                    if (!(Members & (1u << candidate))) { aid = candidate; break; }
                if (!aid) error = 12;
            }
        }
        if (!error && !peer.Connected)
        {
            peer.Connection = connection;
            peer.Connected = true;
            peer.AID = aid;
            peer.MPGeneration = 0;
            peer.MPClosed = false;
            peer.ReceivedSequences.fill(0xFFFF);
            LastSeen(aid, (Console.Timestamp() / 64) | 1);
            Members |= 1u << aid;
            Write16(Status + Layout().Members, Members);
            Write16(Status + Layout().Link, Members ? 3 : 0);
            StoreMAC(Status + Layout().ChildMACs + (aid - 1) * 6, peer.Mac);
            if (Layout().LegacyRetail()) Write16(Status + Layout().Ready, Read16(Status + Layout().Ready) & ~(1u << aid));
            if (Layout().Firmware)
                for (u32 offset = 0; offset < 16; offset += 2)
                    Write16(Status + 0x1F8 + 16 * aid + offset, 1);
            auto result = Result(8);
            Put16(result.Data.data() + 8, 7);
            EncodeMAC(result.Data.data() + 10, peer.Mac);
            Put16(result.Data.data() + 16, aid);
            std::copy_n(data + 20, 24, result.Data.begin() + 20);
            if (Layout().Modern)
            {
                Put16(result.Data.data() + 44, ParentSize);
                Put16(result.Data.data() + 46, ChildSize);
            }
            Queue(std::move(result));
        }
        std::array<u8, 18> accepted{};
        Put64(accepted.data(), connection);
        Put16(accepted.data() + 8, error);
        Put16(accepted.data() + 10, aid);
        Put16(accepted.data() + 12, ParentSize);
        Put16(accepted.data() + 14, ChildSize);
        Put16(accepted.data() + 16, Channel);
        if (!Send(id, Message::Accepted, accepted.data(), accepted.size())) Fail("connect ACK enqueue");
        else if (!error)
        {
            std::array<u8, 18> state{};
            Put64(state.data(), connection);
            Put16(state.data() + 8, MP);
            Put64(state.data() + 10, MPGeneration);
            if (!Send(id, Message::MPState, state.data(), state.size())) Fail("connect MP state enqueue");
        }
        break;
    }
    case Message::Accepted:
    {
        if (size != 18 || !ConnectPending || ParentID != static_cast<int>(id) || U64(data) != ConnectionID) return;
        const u16 error = U16(data + 8), aid = U16(data + 10);
        if (!error && (aid < 1 || aid > 15 || U16(data + 12) > 512 || U16(data + 14) > 512)) return;
        if (!error && Layout().LegacyRetail() && (U16(data + 16) < 1 || U16(data + 16) > 13
            || !(AllowedChannels & (1u << U16(data + 16))))) return;
        ConnectPending = false;
        auto result = Result(12, error);
        if (error) { State(2); Queue(std::move(result)); break; }
        peer.Connected = true;
        peer.Connection = ConnectionID;
        peer.AID = 0;
        peer.MPGeneration = 0;
        peer.MPClosed = false;
        peer.ReceivedSequences.fill(0xFFFF);
        LastSeen(0, (Console.Timestamp() / 64) | 1);
        OwnAID = aid;
        Members = 1;
        ParentSize = MaxParentSize = U16(data + 12);
        ChildSize = MaxChildSize = U16(data + 14);
        Channel = U16(data + 16);
        Write16(Status + Layout().OperatingMode,
            Layout().LegacyRetail() && !(ConnectRequest.Payload[0x4B] & 8) ? 3 : 2);
        Write16(Status + Layout().Members, Members);
        Write16(Status + Layout().Link, Members ? 3 : 0);
        Write16(Status + Layout().AID, OwnAID);
        Write16(Status + Layout().Channel, Channel);
        StoreMAC(Status + Layout().ParentMAC, peer.Mac);
        Write(WM7 + 0x10, ConnectRequest.Payload.data(), ConnectRequest.Payload.size());
        if (Layout().LegacyRetail())
        {
            Write16(Status + Layout().Ready, 1);
            Write16(Status + Layout().Indications, 1);
            Write16(Status + Layout().ConnectedTGID, U16(ConnectRequest.Payload.data() + 0x48));
        }
        State(8);
        if (Profile == 3) RadioClock.Start(Console.Timestamp());
        SetSizes();
        if (Layout().Firmware)
        {
            const u8* bss = ConnectRequest.Payload.data();
            const bool keySharing = bss[0x4B] & 4;
            Write16(Status + Layout().EffectiveTX, U16(bss + 0x4E) + 2 + (keySharing ? 6 : 0));
            Write16(Status + Layout().EffectiveRX, U16(bss + 0x4C) + 4 + (keySharing ? 42 : 0));
        }
        Put16(result.Data.data() + 8, 7);
        Put16(result.Data.data() + 10, OwnAID);
        if (Layout().Modern)
        {
            EncodeMAC(result.Data.data() + 16, peer.Mac);
            Put16(result.Data.data() + 22, ParentSize);
            Put16(result.Data.data() + 24, ChildSize);
        }
        Queue(std::move(result));
        break;
    }
    case Message::Disconnect:
        if (size == 10 && U64(data) == peer.Connection) Disconnect(id, false, U16(data + 8));
        break;
    case Message::MPState:
        if (size == 18 && peer.Connected && U64(data) == peer.Connection && U16(data + 8) <= 1
            && U64(data + 10) >= peer.MPGeneration && (!U16(data + 8)
                || (U64(data + 10) && (U64(data + 10) > peer.MPGeneration || !peer.MPClosed))))
        {
            if (peer.MPGeneration == U64(data + 10) && peer.MP == (U16(data + 8) != 0)) break;
            const bool replaced = peer.MPGeneration != U64(data + 10)
                && (peer.MP || (Parent && peer.MPGeneration));
            peer.MP = U16(data + 8) != 0;
            peer.MPGeneration = U64(data + 10);
            peer.MPClosed = !peer.MP && peer.MPGeneration;
            UpdateMPReady();
            if (!peer.MP || replaced)
            {
                for (auto& operation : Operations)
                {
                    if (!(operation.Pending & (1u << id)) || operation.Connections[id] != peer.Connection) continue;
                    if (Parent && !replaced && ((operation.Port & 8) || operation.Retries)) continue;
                    operation.Pending &= ~(1u << id);
                    operation.Failed |= Parent ? (1u << peer.AID) : 1;
                }
                if (Parent && Round.ID && !Round.Closing)
                {
                    Round.Remaining &= ~(1u << id);
                    Round.Failed |= 1u << peer.AID;
                    if (!Round.Remaining) FinishRound();
                }
                else if (!Parent && Round.ID) FinishChild(1u << OwnAID, true);
                if (!Parent) ClearRounds();
                peer.ReceivedSequences.fill(0xFFFF);
            }
        }
        break;
    case Message::RoundIntent: ReceiveIntent(id, data, size, false); break;
    case Message::RoundCancel: ReceiveCancel(id, data, size); break;
    case Message::RoundPoll: ReceiveIntent(id, data, size, true); break;
    case Message::RoundReply: ReceiveReply(id, data, size); break;
    case Message::RoundAck: ReceiveRoundAck(id, data, size); break;
    case Message::Heartbeat: break;
    default: break;
    }
}

void WMService::Impl::PumpOperations()
{
    const u64 lifetime = OperationLifetime();
    for (auto it = Operations.begin(); it != Operations.end();)
    {
        auto& operation = *it;
        if (!operation.Prepared && ((!operation.Pending)
            || (lifetime && Console.Timestamp() - operation.Started >= lifetime)))
        {
            operation.Failed |= operation.Destinations & ~operation.Sent;
            Complete(operation, operation.Failed ? 15 : 0);
            it = Operations.erase(it);
        }
        else ++it;
    }
    if (!MP || Failed) return;
    UpdateRoundBudget();
    if (Round.ID && !Round.Closing && lifetime && Console.Timestamp() - Round.Started >= lifetime)
    {
        if (Parent) FinishRound();
        else FinishChild(1u << OwnAID, true);
    }
    if (Parent)
    {
        TrySendRound();
        if (Round.ID && !Round.Closing && (Round.Terminating || !Round.Remaining)) FinishRound();
        if (!Round.ID && RoundCount > 0 && RoundLimit > 0 && Console.Timestamp() >= NextSendTick
            && Callbacks.size() < MaxCallbacks - 64) StartRound();
    }
    else
    {
        if (CanPrepareChild())
        {
            ChildKick = false;
            Prepared = PreparePacket(1);
            if (BeaconPhaseValid && Read32(Status + 0x1C) == 1) Prepared.Header |= 0x8000;
            if (ChildPrepareWait) Prepared.WaitUntil = Now() + 25000;
            ChildPrepareWait = false;
        }
        PromoteIntent();
    }
}

bool WMService::Impl::ClaimVBlankWait(u64 deadline)
{
    if (!Parent || Console.VCount() != 192 || CheckedVBlankFrame == *Console.Host.frame) return false;
    CheckedVBlankFrame = *Console.Host.frame;
    if (!Round.ID || !Round.Remaining || Round.Closing || Round.Terminating || InFlight
        || !Callbacks.empty() || Round.Packet.Operations.empty() || Now() >= deadline) return false;
    const u64 tick = Console.Timestamp();
    const auto& ids = Round.Packet.Operations;
    bool granted = false;
    for (auto& operation : Operations)
    {
        if (!operation.Prepared || operation.WaitedAtVBlank || tick < operation.Started
            || tick - operation.Started < 263 * 2130ULL
            || std::find(ids.begin(), ids.end(), operation.ID) == ids.end()) continue;
        operation.WaitedAtVBlank = true;
        granted = true;
    }
    return granted;
}

void WMService::Impl::WaitForMP(u64 vblankDeadline)
{
    if (Mode != Backend::HLE || !MP || Failed || InFlight || !Callbacks.empty()) return;
    const bool parent = Parent && Round.ID && !Round.Closing && !Round.Terminating && (FrameDeadline || vblankDeadline);
    const bool connectedChild = !Parent && ParentID >= 0
        && Peers[ParentID].Connected && Peers[ParentID].MP;
    const bool childPoll = connectedChild && !Round.ID && Prepared.ID;
    const bool childAck = connectedChild && Round.ID && FrameDeadline;
    if (!parent && !childPoll && !childAck) return;
    const bool roundPending = parent || childAck;
    const u64 owner = roundPending ? Round.ID : Prepared.ID;
    const u64 ownedDeadline = roundPending ? Round.WaitUntil : Prepared.WaitUntil;
    u64 deadline = parent && ownedDeadline ? std::min(FrameDeadline, ownedDeadline)
        : ownedDeadline ? ownedDeadline : FrameDeadline;
    if (vblankDeadline) deadline = vblankDeadline;
    if (childAck && Round.VSync)
    {
        constexpr u16 beforeAlarm = LegacyChildMPLine - 1;
        constexpr u64 tail = ((263 - beforeAlarm) * 2130ULL * 1000000 + ClockFrequency - 1) / ClockFrequency;
        if (!TimingLink(Peers[ParentID]) || !BeaconPhaseValid || !Round.Packet.ID
            || Console.VCount() != beforeAlarm || FrameDeadline <= tail) return;
        const u64 slots = std::bitset<16>(Round.PollBitmap & 0xFFFE).count();
        const u64 ackUS = 112 + slots * (10 + Round.ReplySlotTime) + 32 * 8;
        const u64 ackTicks = (ackUS * ClockFrequency + 999999) / 1000000;
        const u64 tick = Console.Timestamp();
        if (tick < Round.Started || tick - Round.Started < ackTicks) return;
        deadline = std::min(deadline, FrameDeadline - tail);
    }
    while (!Failed && !InFlight && Callbacks.empty()
        && (roundPending ? Round.ID == owner && !Round.Closing && !Round.Terminating : !Round.ID && Prepared.ID == owner))
    {
        if (parent) TrySendRound();
        const u64 now = Now();
        if (now >= deadline || Failed || !Callbacks.empty() || PendingBeacons.size() >= MaxPendingBeacons
            || (roundPending && (Round.ID != owner || Round.Closing || Round.Terminating))) break;
        WMServicePacket packet;
        const int received = Platform::MP_WMReceive(packet, Console.UserData);
        if (received < 0) { Fail("MP wait transport receive"); break; }
        if (received) Network(packet);
        else
        {
            const u64 beforeSleep = vblankDeadline ? Now() : now;
            if (beforeSleep < deadline) Platform::Sleep(std::min<u64>(200, deadline - beforeSleep));
        }
    }
}

void WMService::Impl::ReceivePackets(bool host)
{
    if (LocalID < 0) return;
    for (unsigned i = 0; i < 64 && PendingBeacons.size() < MaxPendingBeacons; i++)
    {
        if (host && !CanPollHost()) break;
        if (MP && !Parent && (!Callbacks.empty() || CanPrepareChild())) break;
        WMServicePacket packet;
        const int received = Platform::MP_WMReceive(packet, Console.UserData);
        if (received < 0) { Fail("transport receive capacity"); break; }
        if (!received) break;
        Network(packet);
        if (Terminal) break;
    }
}

void WMService::Impl::PollHost(u64 vblankDeadline)
{
    if (Polling || !CanPollHost()) return;
    if (vblankDeadline && !ClaimVBlankWait(vblankDeadline)) return;
    Polling = true;
    struct PollGuard { bool& Flag; ~PollGuard() { Flag = false; } } guard{Polling};
    if (vblankDeadline) WaitForMP(vblankDeadline);
    else
    {
        Publish();
        ReceivePackets(true);
        if (CanPollHost()) TrySendRound();
    }
    Publish();
}

void WMService::Impl::Tick()
{
    if (Polling) return;
    Polling = true;
    struct PollGuard { bool& Flag; ~PollGuard() { Flag = false; } } guard{Polling};
    Publish();
    if (Console.Timestamp() >= NextNetworkTick)
    {
        NextNetworkTick = Console.Timestamp() + TicksPerMillisecond;
        const auto info = Platform::MP_WMInfo(Console.UserData);
        if (info.Session && !Session)
        {
            if (Mode == Backend::LLE) { Fatal("HLE room attached to LLE execution"); return; }
            Session = info.Session;
            Mode = Backend::HLE;
        }
        if (info.Session && info.Session != Session) { Fatal("HLE room replaced during execution"); return; }
        if (info.Terminal) { Fatal("strict transport violation"); return; }
        Capable = info.Session && info.Session == Session;
        LocalID = info.LocalID;
        for (unsigned id = 0; id < 16; id++)
            if ((Present & (1u << id)) && !(info.Peers & (1u << id)))
            {
                Peers[id].TimingSupportSent = Peers[id].TimingSupportReceived = false;
                if (Initialized) Disconnect(id, false);
            }
        Present = info.Peers;
        if (!Capable && Session && Profile) Fail("HLE room closed");
        ReceivePackets(false);
        if (Terminal) return;
        const u64 now = Now();
        if (Capable && Profile && Mode == Backend::HLE && now >= NextOffer)
        {
            for (unsigned id = 0; id < 16; id++)
            {
                if (!(Present & (1u << id))) continue;
                if (!Negotiated(id)) Offer(id);
                else SendTimingSupport(id);
            }
            NextOffer = now + 100000;
        }
        if (Initialized && Mode == Backend::HLE)
        {
            if (Parent && Console.Timestamp() >= NextBeacon && Callbacks.size() < MaxCallbacks - 8) Advertisement();
            for (unsigned id = 0; id < Peers.size(); id++)
            {
                auto& peer = Peers[id];
                if (now >= peer.NextHeartbeat && Negotiated(id) && (peer.Connected || (PresenceRecipients & (1u << id))))
                {
                    Send(id, Message::Heartbeat);
                    peer.NextHeartbeat = Now() + HeartbeatIntervalUs;
                }
            }
            for (unsigned id = 0; id < 16; id++)
            {
                if (!Peers[id].Connected) continue;
                const u64 elapsed = Console.Timestamp() - Peers[id].SeenEmulated;
                const u16 cam = CAMLifeTimes[Parent ? Peers[id].AID : 1];
                const u64 mpTicks = LifeTime == 0xFFFF ? 0 : LifeTime == 0 ? 64 : (33514ULL * LifeTime * 100 / 64) * 64;
                if ((MP && mpTicks && elapsed > mpTicks)
                    || (cam && cam != 0xFFFF && elapsed > u64(cam) * 100 * TicksPerMillisecond)) Disconnect(id, true);
            }
        }
    }
    if (Terminal) return;
    if (Mode == Backend::HLE && !Initialized && !Closing && !Profile && !Requests.empty())
    {
        if (!Detect(Requests.front())) { Fatal("unsupported replacement WM owner"); return; }
        Mac = Console.Mac();
        AllowedChannels = Console.Channels() & 0x3FFE;
        NewEpoch();
    }
    Drain();
    if (Failed && Initialized && !FailureReported && Callbacks.size() < MaxCallbacks - 64)
    {
        FailureReported = true;
        CancelOperations(15);
        for (unsigned id = 0; id < 16; id++) Disconnect(id, true);
        MP = false;
        Write32(Status + MPOffset(), 0);
        State(2);
        auto result = Result(0x80, 8);
        Put16(result.Data.data() + 4, 22);
        Queue(std::move(result));
    }
    TryScan();
    if (ConnectPending)
    {
        if (Now() >= ConnectDeadline)
        {
            ConnectPending = false;
            State(2);
            Queue(Result(12, 9));
        }
        else TryConnect();
    }
    PumpOperations();
    WaitForMP();
    Publish();
}

WMService::WMService(WMConsole& nds) : State(std::make_unique<Impl>(nds)) {}
WMService::~WMService() = default;
void WMService::Reset()
{
    auto& nds = State->Console;
    for (unsigned id = 0; id < 16; id++)
    {
        const auto& peer = State->Peers[id];
        if (!peer.Connected) continue;
        std::array<u8, 10> data{};
        Put64(data.data(), peer.Connection);
        Put16(data.data() + 8, 3);
        State->Send(id, Message::Disconnect, data.data(), data.size());
    }
    if (State->EverActive)
        Platform::Log(Platform::LogLevel::Info,
            "WMHLE stop epoch=%llu requests=%llu callbacks=%llu received=%llu stale=%llu failed=%u\n",
            static_cast<unsigned long long>(State->Epoch), static_cast<unsigned long long>(State->AcceptedRequests),
            static_cast<unsigned long long>(State->PublishedCallbacks), static_cast<unsigned long long>(State->ReceivedOperations),
            static_cast<unsigned long long>(State->RejectedEpochs), State->Failed);
    State = std::make_unique<Impl>(nds);
    Platform::MP_WMReset(nds.UserData);
}
void WMService::StopIfTerminal()
{
    if (!State->Terminal || State->StopRequested) return;
    State->StopRequested = true;
    auto& nds = State->Console;
    Platform::MP_WMAbort(nds.UserData);
    nds.Stop();
}
void WMService::Poll() { State->Tick(); StopIfTerminal(); }
void WMService::PollHost() { State->PollHost(); StopIfTerminal(); }
void WMService::WaitAtVBlank(u64 edge) { State->PollHost(edge + 4000); StopIfTerminal(); }
bool WMService::CanPollHost() const { return State->CanPollHost(); }
bool WMService::NeedsGuestCycles() const
{
    return State->CanPollHost() && (State->InFlight || !State->Callbacks.empty()
        || State->CanPrepareChild() || State->PendingBeacons.size() >= MaxPendingBeacons
        || (!State->Parent && !State->Round.ID && !State->Prepared.ID
            && (State->ChildVSync || (!State->Intent.Data.empty()
                && State->Intent.State == RoundIntent::Stage::Pending))));
}
void WMService::SetFrameDeadline(u64 deadline) { State->FrameDeadline = deadline; }
void WMService::Capture(u32 word) { State->Capture(word); StopIfTerminal(); }
void WMService::ClearFIFO() { State->Requests.clear(); }
bool WMService::BlocksARM7() const { return State->BlocksARM7(); }
bool WMService::Active() const { return State->EverActive || State->Profile || State->Terminal; }
}
