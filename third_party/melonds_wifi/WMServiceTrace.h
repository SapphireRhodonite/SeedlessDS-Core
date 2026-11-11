#ifndef MELONDS_WMSERVICETRACE_H
#define MELONDS_WMSERVICETRACE_H

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include "types.h"

namespace melonDS
{

struct WMServiceTraceMetadata
{
    enum class Stage : u32
    {
        GameInfo = 1,
        Transmit = 2,
        Receive = 3,
        NetworkDecision = 4,
        ScanWindow = 5,
        ScanDecision = 6,
        Descriptor = 7,
    };
    enum class Reason : u32
    {
        None = 0,
        Applied = 1,
        Enqueued = 2,
        EnqueueFailed = 3,
        Incapable = 4,
        PeerID = 5,
        Absent = 6,
        TooLong = 7,
        HeaderShort = 8,
        Self = 9,
        Magic = 10,
        Version = 11,
        Reserved = 12,
        SenderEpochZero = 13,
        ReceiverEpoch = 14,
        PeerEpoch = 15,
        OfferReceiver = 16,
        OfferSize = 17,
        OfferReserved = 18,
        OfferProfile = 19,
        OfferIdentity = 20,
        OfferLLE = 21,
        OfferUndecided = 22,
        OfferNoProfile = 23,
        OfferAccepted = 24,
        Rejected = 25,
        ReadySize = 26,
        ReadyLLE = 27,
        ReadyUndecided = 28,
        ReadyAccepted = 29,
        NotHLE = 30,
        NotInitialized = 31,
        PeerNotReady = 32,
        Failed = 33,
        AdvertisementSize = 34,
        GameLength = 35,
        Channel = 36,
        PeerMAC = 37,
        AdvertisementAccepted = 38,
        ScanStart = 39,
        ScanAdvance = 40,
        ScanFinish = 41,
        ScanInvalidated = 42,
        NoScan = 43,
        ScanAPI = 44,
        ScanEpoch = 45,
        ScanOwner = 46,
        ScanExpired = 47,
        NotNegotiated = 48,
        BSSSize = 49,
        BSSLength = 50,
        SSIDLength = 51,
        SSIDMinimum = 52,
        BSSGameLength = 53,
        ScanChannel = 54,
        BSSPeerMAC = 55,
        MACFilter = 56,
        SSIDFilter = 57,
        Duplicate = 58,
        ObservationFull = 59,
        Budget = 60,
        Capacity = 61,
        ScanAccepted = 62,
        ScanTokenMissing = 63,
        ProbeSize = 64,
        ProbeToken = 65,
        ProbeReserved = 66,
        NotParent = 67,
        ParentState = 68,
        NoGameInfo = 69,
        ProbeChannel = 70,
        ProbeSSIDLength = 71,
        ProbeMAC = 72,
        ProbeSSID = 73,
        ProbeAccepted = 74,
        ResponseSize = 75,
        PassiveScan = 76,
        ResponseToken = 77,
        ResponseAccepted = 78,
    };
    enum ValidField : u32
    {
        OwnerValid = 1 << 0,
        PeerValid = 1 << 1,
        ScanValid = 1 << 2,
        CauseValid = 1 << 3,
        ConnectionValid = 1 << 4,
        RequestValid = 1 << 5,
        LengthValid = 1 << 6,
        DescriptorValid = 1 << 7,
        PacketValid = 1 << 8,
    };
    Stage Event;
    Reason Result;
    u64 CauseSequence = ~u64{0};
    u64 Session = 0;
    u64 Epoch = 0;
    u64 PeerEpoch = 0;
    u64 Connection = 0;
    u64 ScanToken = 0;
    u64 ScanEpoch = 0;
    u64 ScanDeadline = 0;
    u64 NextBeacon = 0;
    u32 Valid = 0;
    u32 LocalID = ~u32{0};
    u32 PeerID = ~u32{0};
    u32 WM9 = 0;
    u32 ScanOwner = 0;
    u32 State = 0;
    u32 Mode = 0;
    u32 ServiceFlags = 0;
    u32 ScanMask = 0;
    u32 ScanChannel = 0;
    u32 ScanDwell = 0;
    u32 ScanInterval = 0;
    u32 ScanElapsed = 0;
    u32 ScanCount = 0;
    u32 ScanBytes = 0;
    u32 ScanBudget = 0;
    u32 Profile = 0;
    u32 RequestAddress = 0;
    u32 API = 0;
    u32 Message = 0;
    u32 Index = 0;
    u32 Count = 0;
    u32 SourceKind = 0;
    u32 DeclaredLength = 0;
    u32 CallbackCount = 0;
    u32 NegotiatedMask = 0;
    u32 RecordFlags = 0;

    WMServiceTraceMetadata(Stage stage, Reason reason, u32 peer = ~u32{0}, u64 cause = ~u64{0})
        : Event(stage), Result(reason), CauseSequence(cause), PeerID(peer)
    {
        if (cause != ~u64{0}) Valid |= CauseValid;
    }
};

struct WMServiceTrace
{
    static constexpr u32 Capacity = 65536;
    static constexpr u32 BufferCapacity = 256;
    static constexpr u32 DataCapacity = 512;

    enum EventKind : u32
    {
        ARM9Send,
        ARM7Receive,
        ARM7Send,
        ARM9Receive,
        Service,
        Descriptor,
    };

    enum EventFlag : u32
    {
        Enabled = 1 << 0,
        Available = 1 << 1,
        Accepted = 1 << 2,
        PXIError = 1 << 3,
        ApproximateRegisters = 1 << 4,
        InvalidBuffer = 1 << 5,
        InvalidData = 1 << 6,
        TruncatedData = 1 << 7,
    };

    struct Event
    {
        u64 Sequence;
        u64 HostNanoseconds;
        u64 SystemTimestamp;
        u64 ARM9Timestamp;
        u64 ARM7Timestamp;
        u32 Frame;
        u32 Kind;
        u32 Word;
        u32 BufferAddress;
        u32 Flags;
        u32 PC9, LR9, SP9, CPSR9;
        u32 PC7, LR7, SP7, CPSR7;
        u32 FIFOCnt9, FIFOCnt7, FIFOLevel9, FIFOLevel7;
        u32 CurCPU;
        u32 BufferCopied;
        u32 DataAddress;
        u32 DataLength;
        u32 DataCopied;
        u32 PayloadFormat;
        u32 WRAMCnt;
        u8 Buffer[BufferCapacity];
        u8 Data[DataCapacity];
    };

    struct FileHeader
    {
        char Magic[8];
        u32 Version;
        u32 HeaderBytes;
        u32 EventBytes;
        u32 SnapshotHeaderBytes;
        u64 Owner;
        u64 Epoch;
        u64 EventCount;
        u64 Seen;
        u64 Dropped;
        u64 InvalidBuffers;
        u64 InvalidPayloads;
        u64 TruncatedPayloads;
        u64 SnapshotBytes;
        u64 SnapshotOffset;
        u64 HostBeginNanoseconds;
        u64 HostEndNanoseconds;
        u64 EmuBeginTimestamp;
        u64 EmuEndTimestamp;
        u32 BeginFrame;
        u32 EndFrame;
        u32 ConsoleType;
        u32 Flags;
        u32 StopReason;
        u32 Endian;
        char CaptureId[64];
    };

    struct CPURegisters
    {
        u32 R[16];
        u32 CPSR;
        u32 R_FIQ[8];
        u32 R_SVC[3];
        u32 R_ABT[3];
        u32 R_IRQ[3];
        u32 R_UND[3];
        u32 CurInstr;
        u32 NextInstr[2];
        u32 ExceptionBase;
    };

    struct SnapshotHeader
    {
        u64 HostNanoseconds;
        u64 SystemTimestamp;
        u64 ARM9Timestamp;
        u64 ARM7Timestamp;
        u64 NextSequence;
        u32 Frame;
        u32 CPUStop;
        u32 ARM9ClockShift;
        u32 WRAMCnt;
        u32 MainRAMMask;
        u32 Shared9Offset, Shared9Mask, Shared7Offset, Shared7Mask;
        u32 CP15Control, ITCMSetting, DTCMSetting, ITCMSize, DTCMBase, DTCMMask;
        u32 MemorySizes[14];
        u32 FIFOCnt[2], FIFOLevel[2], IPCSync[2];
        u32 FIFO9[16], FIFO7[16];
        u32 IME[2], IE[2], IF[2];
        CPURegisters ARM9, ARM7;
        u32 VRAMCNT[9];
        u32 VRAMSTAT;
        u32 VRAMMap_LCDC;
        u32 VRAMMap_ABG[32];
        u32 VRAMMap_AOBJ[16];
        u32 VRAMMap_BBG[8];
        u32 VRAMMap_BOBJ[8];
        u32 VRAMMap_ABGExtPal[4];
        u32 VRAMMap_AOBJExtPal;
        u32 VRAMMap_BBGExtPal[4];
        u32 VRAMMap_BOBJExtPal;
        u32 VRAMMap_Texture[4];
        u32 VRAMMap_TexPal[8];
        u32 VRAMMap_ARM7[2];
    };

    struct Footer
    {
        char Magic[8];
        u64 FileBytes;
        u64 EventCount;
        u64 Dropped;
    };

    static u64 Now()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static bool ValidId(const char* id)
    {
        const size_t length = std::strlen(id);
        if (!length || length >= sizeof(FileHeader::CaptureId) || !std::strcmp(id, "0"))
            return false;
        for (size_t i = 0; i < length; ++i)
        {
            const char c = id[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_' || c == '-'))
                return false;
        }
        return true;
    }

    bool Begin(const std::string& directory, const char* id, u64 owner,
               u32 consoleType, bool jit, u32 frame, u64 timestamp)
    {
        if (FlushFailed)
            return false;
        if (!Events)
            Events.reset(new (std::nothrow) Event[Capacity]);
        if (!Events)
            return false;
        Snapshot.reset();
        SnapshotMeta = {};
        File = {};
        std::memcpy(File.Magic, "WMTRACE1", 8);
        File.Version = 2;
        File.HeaderBytes = sizeof(FileHeader);
        File.EventBytes = sizeof(Event);
        File.Owner = owner;
        File.Epoch = ++Epoch;
        File.HostBeginNanoseconds = Now();
        File.EmuBeginTimestamp = timestamp;
        File.BeginFrame = frame;
        File.ConsoleType = consoleType;
        File.Flags = jit ? 1 : 0;
        File.Endian = 0x12345678;
        std::strcpy(File.CaptureId, id);
        Path = directory + "/wm-service-" + id + "-"
            + std::to_string(File.HostBeginNanoseconds) + "-"
            + std::to_string(owner) + "-" + std::to_string(Epoch) + ".bin";
        Active = true;
        return true;
    }

    bool Finish(u32 frame, u64 timestamp, u32 reason, FILE* (*openFile)(const char*, const char*))
    {
        if (!Active)
            return !FlushFailed;
        Active = false;
        File.EndFrame = frame;
        File.EmuEndTimestamp = timestamp;
        File.HostEndNanoseconds = Now();
        File.StopReason = reason;
        if (Snapshot)
        {
            File.SnapshotOffset = sizeof(FileHeader) + File.EventCount * sizeof(Event);
            File.SnapshotHeaderBytes = sizeof(SnapshotHeader);
        }
        const Footer footer{{'W', 'M', 'E', 'N', 'D', '0', '0', '1'},
            sizeof(FileHeader) + File.EventCount * sizeof(Event)
                + File.SnapshotHeaderBytes + File.SnapshotBytes + sizeof(Footer),
            File.EventCount, File.Dropped};
        std::unique_ptr<FILE, decltype(&std::fclose)> output(
            openFile(Path.c_str(), "wbx"), &std::fclose);
        bool ok = output != nullptr;
        if (ok)
            ok = std::fwrite(&File, sizeof(File), 1, output.get()) == 1;
        if (ok && File.EventCount)
            ok = std::fwrite(Events.get(), sizeof(Event), File.EventCount, output.get()) == File.EventCount;
        if (ok && Snapshot)
            ok = std::fwrite(&SnapshotMeta, sizeof(SnapshotMeta), 1, output.get()) == 1
                && std::fwrite(Snapshot.get(), 1, File.SnapshotBytes, output.get()) == File.SnapshotBytes;
        if (ok)
            ok = std::fwrite(&footer, sizeof(footer), 1, output.get()) == 1;
        if (output)
        {
            if (std::fflush(output.get()) != 0)
                ok = false;
            if (std::fclose(output.release()) != 0)
                ok = false;
        }
        FlushFailed = !ok;
        if (ok)
        {
            Events.reset();
            Snapshot.reset();
        }
        return ok;
    }

    Event* Reserve()
    {
        if (!Active) return nullptr;
        const u64 sequence = File.Seen++;
        if (File.EventCount >= Capacity)
        {
            ++File.Dropped;
            return nullptr;
        }
        auto& event = Events[File.EventCount++];
        event = {};
        event.Sequence = sequence;
        return &event;
    }

    void Flag(Event& event, u32 flag)
    {
        if (event.Flags & flag) return;
        event.Flags |= flag;
        if (flag == InvalidBuffer) ++File.InvalidBuffers;
        else if (flag == InvalidData) ++File.InvalidPayloads;
        else if (flag == TruncatedData) ++File.TruncatedPayloads;
    }

    static void Metadata(Event& event, const WMServiceTraceMetadata& metadata, u32 length, u32 extraLength)
    {
        u32 offset = 0;
        const auto put32 = [&](u32 value) {
            for (unsigned i = 0; i < 4; ++i) event.Buffer[offset++] = value >> (i * 8);
        };
        const auto put64 = [&](u64 value) {
            for (unsigned i = 0; i < 8; ++i) event.Buffer[offset++] = value >> (i * 8);
        };
        put64(metadata.CauseSequence);
        put64(metadata.Session);
        put64(metadata.Epoch);
        put64(metadata.PeerEpoch);
        put64(metadata.Connection);
        put64(metadata.ScanToken);
        put64(metadata.ScanEpoch);
        put64(metadata.ScanDeadline);
        put64(metadata.NextBeacon);
        put32(1);
        put32(static_cast<u32>(metadata.Event));
        put32(static_cast<u32>(metadata.Result));
        put32(metadata.Valid);
        put32(metadata.LocalID);
        put32(metadata.PeerID);
        put32(metadata.WM9);
        put32(metadata.ScanOwner);
        put32(metadata.State);
        put32(metadata.Mode);
        put32(metadata.ServiceFlags);
        put32(metadata.ScanMask);
        put32(metadata.ScanChannel);
        put32(metadata.ScanDwell);
        put32(metadata.ScanInterval);
        put32(metadata.ScanElapsed);
        put32(metadata.ScanCount);
        put32(metadata.ScanBytes);
        put32(metadata.ScanBudget);
        put32(metadata.Profile);
        put32(metadata.RequestAddress);
        put32(metadata.API);
        put32(metadata.Message);
        put32(metadata.Index);
        put32(metadata.Count);
        put32(metadata.SourceKind);
        put32(metadata.DeclaredLength);
        put32(metadata.CallbackCount);
        put32(metadata.NegotiatedMask);
        put32(length);
        put32(extraLength);
        event.BufferCopied = offset;
        event.PayloadFormat = static_cast<u32>(metadata.Event);
    }

    std::unique_ptr<Event[]> Events;
    std::unique_ptr<u8[]> Snapshot;
    FileHeader File{};
    SnapshotHeader SnapshotMeta{};
    std::string LastRequest;
    std::string Path;
    u64 Epoch = 0;
    bool Active = false;
    bool FlushFailed = false;
};

static_assert(sizeof(WMServiceTrace::Event) == 904);
static_assert(offsetof(WMServiceTrace::Event, Buffer) == 136);
static_assert(sizeof(WMServiceTrace::FileHeader) == 224);
static_assert(sizeof(WMServiceTrace::CPURegisters) == 164);
static_assert(sizeof(WMServiceTrace::SnapshotHeader) == 1056);
static_assert(sizeof(WMServiceTrace::Footer) == 32);

}

#endif
