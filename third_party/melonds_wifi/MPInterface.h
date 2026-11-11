/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef MPINTERFACE_H
#define MPINTERFACE_H

#include <memory>
#include <array>
#include "types.h"

namespace melonDS
{

// TODO: provision for excluding unwanted interfaces at compile time
enum MPInterfaceType
{
    MPInterface_Dummy = -1,
    MPInterface_Local,
    MPInterface_LAN,
    MPInterface_Netplay,
};

struct MPPacketHeader
{
    u32 Magic;
    u32 SenderID;
    u32 Type;       // 0=regular 1=CMD 2=reply 3=ack
    u32 Length;
    u64 Timestamp;
};

inline constexpr u8 WMServiceVersion = 8;

struct WMServiceInfo
{
    u64 Session = 0;
    int LocalID = -1;
    u16 Peers = 0;
    bool Terminal = false;
};

struct WMServicePacket
{
    u8 SenderID = 0;
    u64 Connection = 0;
    u16 Length = 0;
    std::array<u8, 2048> Data{};
};

class MPInterface
{
public:
    virtual ~MPInterface() = default;

    static MPInterface& Get() { return *Current; }
    static MPInterfaceType GetType() { return CurrentType; }
    static void Set(MPInterfaceType type);

    [[nodiscard]] int GetRecvTimeout() const noexcept { return RecvTimeout; }
    void SetRecvTimeout(int timeout) noexcept { RecvTimeout = timeout; }

    // function called every video frame
    virtual void Process() = 0;

    virtual void Begin(int inst) = 0;
    virtual void End(int inst) = 0;

    virtual int SendPacket(int inst, u8* data, int len, u64 timestamp) = 0;
    virtual int RecvPacket(int inst, u8* data, u64* timestamp) = 0;
    virtual int SendCmd(int inst, u8* data, int len, u64 timestamp) = 0;
    virtual int SendReply(int inst, u8* data, int len, u64 timestamp, u16 aid) = 0;
    virtual int SendAck(int inst, u8* data, int len, u64 timestamp) = 0;
    virtual int SendProgress(int inst, u64 timestamp) { return 0; }
    virtual bool UsesConservativeAck() const { return false; }
    virtual int RecvHostPacket(int inst, u8* data, u64* timestamp) = 0;
    virtual u16 RecvReplies(int inst, u8* data, u64 timestamp, u16 aidmask) = 0;
    virtual bool SupportsWMService() const { return false; }
    virtual WMServiceInfo GetWMServiceInfo() const { return {}; }
    virtual int GetWMServiceLocalID() const { return -1; }
    virtual u16 GetWMServicePeerMask() const { return 0; }
    virtual u16 SendWMService(u16 peers, const u8* data, u16 length) { return 0; }
    virtual int ReceiveWMService(WMServicePacket& packet) { return 0; }
    virtual void ResetWMService() {}

protected:
    int RecvTimeout = 25;

private:
    static MPInterfaceType CurrentType;
    static std::unique_ptr<MPInterface> Current;
};

}

#endif // MPINTERFACE_H
