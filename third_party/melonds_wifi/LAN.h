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

#ifndef LAN_H
#define LAN_H

#include <string>
#include <vector>
#include <map>
#include <queue>

#include <enet/enet.h>

#ifndef socket_t
    #ifdef __WIN32__
        #include <winsock2.h>
        #define socket_t    SOCKET
    #else
        #define socket_t    int
    #endif
#endif

#include "types.h"
#include "Platform.h"
#include "MPInterface.h"

namespace melonDS
{

class LAN : public MPInterface
{
public:
    LAN() noexcept;
    LAN(const LAN&) = delete;
    LAN& operator=(const LAN&) = delete;
    LAN(LAN&& other) = delete;
    LAN& operator=(LAN&& other) = delete;
    ~LAN() noexcept;

    enum PlayerStatus : int
    {
        Player_None = 0,        // no player in this entry
        Player_Client,          // game client
        Player_Host,            // game host
        Player_Connecting,      // player still connecting
        Player_Disconnected,    // player disconnected
    };

    struct Player
    {
        int ID;
        char Name[32];
        PlayerStatus Status;
        u32 Address;

        bool IsLocalPlayer;
        u16 Port;
        u32 Ping;
    };

    struct DiscoveryData
    {
        u32 Magic;
        u32 Version;
        u32 Tick;
        char SessionName[64];
        u8 NumPlayers;
        u8 MaxPlayers;
        u8 Status; // 0=idle 1=playing
    };

    struct GroupInterface
    {
        u32 LocalAddress;
        u32 OwnerAddress;
        unsigned Index;
    };

    bool StartDiscovery();
    void EndDiscovery();
    bool StartHost(const char* player, int numplayers, bool hle = false, const GroupInterface* group = nullptr);
    bool StartClient(const char* player, const char* host, bool hle = false, const GroupInterface* group = nullptr);
    void EndSession();
    void SetFirmwareBoot(bool firmware);
    u16 GetFirmwareBootMask() const { return FirmwareBootMask; }
    bool UsesConservativeAck() const override { return false; }

    std::map<u32, DiscoveryData> GetDiscoveryList();
    std::vector<Player> GetPlayerList();
    int GetNumPlayers() { return NumPlayers; }
    int GetMaxPlayers() { return MaxPlayers; }

    void Process() override;

    void Begin(int inst) override;
    void End(int inst) override;

    int SendPacket(int inst, u8* data, int len, u64 timestamp) override;
    int RecvPacket(int inst, u8* data, u64* timestamp) override;
    int SendCmd(int inst, u8* data, int len, u64 timestamp) override;
    int SendReply(int inst, u8* data, int len, u64 timestamp, u16 aid) override;
    int SendAck(int inst, u8* data, int len, u64 timestamp) override;
    int SendProgress(int inst, u64 timestamp) override;
    int RecvHostPacket(int inst, u8* data, u64* timestamp) override;
    u16 RecvReplies(int inst, u8* data, u64 timestamp, u16 aidmask) override;
    bool SupportsWMService() const override;
    WMServiceInfo GetWMServiceInfo() const override;
    int GetWMServiceLocalID() const override;
    u16 GetWMServicePeerMask() const override;
    u16 SendWMService(u16 peers, const u8* data, u16 length) override;
    int ReceiveWMService(WMServicePacket& packet) override;
    void ResetWMService() override;

    struct ReceiveWait
    {
        void* Context;
        void (*Callback)(void* context, int timeout);
    };

    ENetSocket GetSocket() const { return Host ? Host->socket : ENET_SOCKET_NULL; }
    void SetFlushOnSend(bool enabled) { FlushOnSend = enabled; }
    void Flush() { if (Host) enet_host_flush(Host); }
    size_t GetReceiveQueueSize() const { return RXQueue.size(); }
    bool HasPendingWMDispatch() const { return Active && WMServiceHLE && Host && !enet_list_empty(&Host->dispatchQueue); }
    bool HasPendingPacket() const { return !RXQueue.empty() || !RXDeferredQueue.empty(); }
    int RecvPacket(int inst, u8* data, u64* timestamp, const ReceiveWait* wait);
    int RecvHostPacket(int inst, u8* data, u64* timestamp, const ReceiveWait* wait);
    u16 RecvReplies(int inst, u8* data, u64 timestamp, u16 aidmask, const ReceiveWait* wait);

private:
    bool Inited;
    bool Active;
    bool IsHost;
    bool FlushOnSend = true;

    ENetHost* Host;
    ENetPeer* RemotePeers[16];

    socket_t DiscoverySocket;
    u32 DiscoveryLastTick;
    std::map<u32, DiscoveryData> DiscoveryList;
    Platform::Mutex* DiscoveryMutex;

    Player Players[16];
    int NumPlayers;
    int MaxPlayers;
    Platform::Mutex* PlayersMutex;

    Player MyPlayer;
    u32 HostAddress;

    u16 ConnectedBitmask;
    u16 FirmwareBootMask;
    u16 WMServiceCapabilities = 0;
    bool WMServiceHLE = false;
    bool WMServiceTerminal = false;
    u64 WMServiceSession = 0;
    u64 WMConnectionCounter = 0;
    std::array<u64, 16> WMConnections{};
    bool WMServiceReceiveFailed = false;
    std::queue<WMServicePacket> WMServiceReceiveQueue;

    int MPRecvTimeout;
    int LastHostID;
    ENetPeer* LastHostPeer;
    std::queue<ENetPacket*> RXQueue;
    std::queue<ENetPacket*> RXDeferredQueue;

    u32 FrameCount;

    void ProcessDiscovery();
    void ClearReceiveQueue();
    void SendFirmwareBoot();
    void SendWMServiceCapability(ENetPeer* peer);
    void ProcessWMServiceEvent(const ENetEvent& event);

    void HostUpdatePlayerList();
    void ClientUpdatePlayerList();

    void ProcessHostEvent(ENetEvent& event);
    void ProcessClientEvent(ENetEvent& event);
    void ProcessEvent(ENetEvent& event);
    void ProcessLAN(int type, const ReceiveWait* wait = nullptr);

    int SendPacketGeneric(u32 type, u8* packet, int len, u64 timestamp);
    int RecvPacketGeneric(u8* packet, bool block, u64* timestamp, const ReceiveWait* wait = nullptr);
};

}

#endif // LAN_H
