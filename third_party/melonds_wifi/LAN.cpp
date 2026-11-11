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

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <array>

#ifdef __WIN32__
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #define socket_t    SOCKET
    #define sockaddr_t  SOCKADDR
    #define sockaddr_in_t  SOCKADDR_IN
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <net/if.h>

    #define socket_t    int
    #define sockaddr_t  struct sockaddr
    #define sockaddr_in_t  struct sockaddr_in
    #define closesocket close
#endif

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  (socket_t)-1
#endif

#include "LAN.h"
#include <atomic>
#include <chrono>


namespace melonDS
{

const u32 kDiscoveryMagic = 0x444E414C; // LAND
const u32 kLANMagic = 0x504E414C; // LANP
const u32 kPacketMagic = 0x4946494E; // NIFI

const u32 kProtocolVersion = 6;

static u64 NewWMSession()
{
    static std::atomic<u64> counter{0};
    const u64 now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    u64 previous = counter.load(std::memory_order_relaxed);
    u64 next;
    do { next = std::max(previous + 1, now); }
    while (!counter.compare_exchange_weak(previous, next, std::memory_order_relaxed));
    return next;
}

static void WriteWMSession(u8* data, bool hle, u64 session)
{
    data[0] = hle ? 1 : 0;
    data[1] = WMServiceVersion;
    for (unsigned i = 0; i < 8; ++i) data[2 + i] = session >> (i * 8);
}

static u64 ReadWMSession(const u8* data)
{
    u64 session = 0;
    for (unsigned i = 0; i < 8; ++i) session |= u64(data[2 + i]) << (i * 8);
    return session;
}

const u32 kLocalhost = 0x0100007F;

enum
{
    Chan_Cmd = 0,           // channel 0 -- control commands
    Chan_MP,                // channel 1 -- MP data exchange
};

enum
{
    Cmd_ClientInit = 1,     // 01 -- host->client -- init new client and assign ID
    Cmd_PlayerInfo,         // 02 -- client->host -- send client player info to host
    Cmd_PlayerList,         // 03 -- host->client -- broadcast updated player list
    Cmd_PlayerConnect,      // 04 -- both -- signal connected state (ready to receive MP frames)
    Cmd_PlayerDisconnect,   // 05 -- both -- signal disconnected state (not receiving MP frames)
    Cmd_WMServiceCapability,
    Cmd_WMServiceData,
};

const int kDiscoveryPort = 7063;
const int kLANPort = 7064;

static void SetTrafficPriority(ENetHost* host)
{
    const int priority = 0xC0;
    if (setsockopt(host->socket, IPPROTO_IP, IP_TOS,
                   reinterpret_cast<const char*>(&priority), sizeof(priority)) < 0)
        Platform::Log(Platform::LogLevel::Warn, "LAN: traffic priority unavailable\n");
}

static std::vector<u32> BroadcastAddresses(socket_t socket)
{
    std::vector<u32> addresses;
#ifndef __WIN32__
    std::array<char, 8192> buffer{};
    ifconf configuration{};
    configuration.ifc_len = buffer.size();
    configuration.ifc_buf = buffer.data();
    if (ioctl(socket, SIOCGIFCONF, &configuration) == 0)
    {
        size_t offset = 0;
        while (offset + sizeof(ifreq) <= static_cast<size_t>(configuration.ifc_len))
        {
            ifreq interface{};
            memcpy(&interface, buffer.data() + offset, sizeof(interface));
            size_t length = sizeof(interface);
#ifdef __APPLE__
            length = IFNAMSIZ + std::max(sizeof(sockaddr), static_cast<size_t>(interface.ifr_addr.sa_len));
#endif
            if (length > static_cast<size_t>(configuration.ifc_len) - offset) break;
            offset += length;
            if (interface.ifr_addr.sa_family != AF_INET) continue;
            interface.ifr_name[IFNAMSIZ - 1] = '\0';
            if (ioctl(socket, SIOCGIFFLAGS, &interface) != 0) continue;
            if ((interface.ifr_flags & (IFF_UP | IFF_BROADCAST)) != (IFF_UP | IFF_BROADCAST)) continue;
            if (ioctl(socket, SIOCGIFBRDADDR, &interface) != 0) continue;
            sockaddr_in destination{};
            memcpy(&destination, &interface.ifr_broadaddr, sizeof(destination));
            const u32 address = destination.sin_addr.s_addr;
            if (address && std::find(addresses.begin(), addresses.end(), address) == addresses.end())
                addresses.push_back(address);
        }
    }
#endif
    if (addresses.empty()) addresses.push_back(htonl(INADDR_BROADCAST));
    return addresses;
}


LAN::LAN() noexcept : Inited(false)
{
    DiscoveryMutex = Platform::Mutex_Create();
    PlayersMutex = Platform::Mutex_Create();

    DiscoverySocket = INVALID_SOCKET;
    DiscoveryLastTick = 0;

    Active = false;
    IsHost = false;
    Host = nullptr;
    //Lag = false;

    memset(RemotePeers, 0, sizeof(RemotePeers));
    memset(Players, 0, sizeof(Players));
    memset(&MyPlayer, 0, sizeof(MyPlayer));
    NumPlayers = 0;
    MaxPlayers = 0;

    ConnectedBitmask = 0;
    FirmwareBootMask = 0;

    MPRecvTimeout = 25;
    LastHostID = -1;
    LastHostPeer = nullptr;

    FrameCount = 0;

    // TODO make this somewhat nicer
    if (enet_initialize() != 0)
    {
        Platform::Log(Platform::LogLevel::Error, "LAN: failed to initialize enet\n");
        return;
    }

    Platform::Log(Platform::LogLevel::Info, "LAN: enet initialized\n");
    Inited = true;
}

LAN::~LAN() noexcept
{
    EndSession();

    Inited = false;
    enet_deinitialize();

    Platform::Mutex_Free(DiscoveryMutex);
    Platform::Mutex_Free(PlayersMutex);

    Platform::Log(Platform::LogLevel::Info, "LAN: enet deinitialized\n");
}


std::map<u32, LAN::DiscoveryData> LAN::GetDiscoveryList()
{
    Platform::Mutex_Lock(DiscoveryMutex);
    auto ret = DiscoveryList;
    Platform::Mutex_Unlock(DiscoveryMutex);
    return ret;
}

std::vector<LAN::Player> LAN::GetPlayerList()
{
    Platform::Mutex_Lock(PlayersMutex);

    std::vector<Player> ret;
    for (int i = 0; i < 16; i++)
    {
        if (Players[i].Status == Player_None) continue;

        // make a copy of the player entry, fix up the address field
        Player newp = Players[i];
        if (newp.ID == MyPlayer.ID)
        {
            newp.IsLocalPlayer = true;
            newp.Address = kLocalhost;
        }
        else
        {
            newp.IsLocalPlayer = false;
            if (newp.Status == Player_Host)
                newp.Address = HostAddress;
        }

        ret.push_back(newp);
    }

    Platform::Mutex_Unlock(PlayersMutex);
    return ret;
}


bool LAN::StartDiscovery()
{
    if (!Inited) return false;
    if (DiscoverySocket != INVALID_SOCKET) return true;

    int res;

    DiscoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (DiscoverySocket < 0)
    {
        DiscoverySocket = INVALID_SOCKET;
        return false;
    }

    sockaddr_in_t saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(kDiscoveryPort);
    int reuse = 1;
    setsockopt(DiscoverySocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(DiscoverySocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif
    res = bind(DiscoverySocket, (const sockaddr_t*)&saddr, sizeof(saddr));
    if (res < 0)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
        return false;
    }

    int opt_true = 1;
    res = setsockopt(DiscoverySocket, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    if (res < 0)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
        return false;
    }

    DiscoveryLastTick = (u32)Platform::GetMSCount();
    DiscoveryList.clear();

    Active = true;
    return true;
}

void LAN::EndDiscovery()
{
    if (!Inited) return;

    if (DiscoverySocket != INVALID_SOCKET)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
    }

    if (!IsHost)
        Active = false;
}

bool LAN::StartHost(const char* playername, int numplayers, bool hle, const GroupInterface* group)
{
    if (!Inited) return false;
    if (numplayers < 2 || numplayers > 16 || Host) return false;
    WMServiceHLE = hle;
    WMServiceSession = NewWMSession();
    WMServiceTerminal = false;

    ENetAddress addr;
    addr.host = group ? group->LocalAddress : ENET_HOST_ANY;
    addr.port = kLANPort;

    Host = enet_host_create_on_interface(&addr, 16, 2, 0, 0, group ? group->Index : 0);
    if (!Host)
    {
        return false;
    }
    SetTrafficPriority(Host);

    Platform::Mutex_Lock(PlayersMutex);

    Player* player = &Players[0];
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = Player_Host;
    player->Address = kLocalhost;
    player->Port = kLANPort;
    NumPlayers = 1;
    MaxPlayers = numplayers;
    memcpy(&MyPlayer, player, sizeof(Player));

    Platform::Mutex_Unlock(PlayersMutex);

    HostAddress = kLocalhost;
    LastHostID = -1;
    LastHostPeer = nullptr;

    Active = true;
    IsHost = true;

    if (!group) StartDiscovery();
    return true;
}

bool LAN::StartClient(const char* playername, const char* host, bool hle, const GroupInterface* group)
{
    if (!Inited || Host) return false;
    WMServiceHLE = hle;
    WMServiceSession = 0;
    WMServiceTerminal = false;

    ENetAddress local{group ? group->LocalAddress : ENET_HOST_ANY, 0};
    Host = enet_host_create_on_interface(&local, 16, 2, 0, 0, group ? group->Index : 0);
    if (!Host)
    {
        return false;
    }
    SetTrafficPriority(Host);

    ENetAddress addr;
    if (group) addr.host = group->OwnerAddress;
    else if (enet_address_set_host(&addr, host) != 0)
    {
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }
    addr.port = kLANPort;
    ENetPeer* peer = enet_host_connect(Host, &addr, 2, 0);
    if (!peer)
    {
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }

    Platform::Mutex_Lock(PlayersMutex);

    Player* player = &MyPlayer;
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = Player_Connecting;
    player->Port = Host->address.port;

    Platform::Mutex_Unlock(PlayersMutex);

    ENetEvent event;
    int conn = 0;
    u32 starttick = (u32)Platform::GetMSCount();
    const int conntimeout = 5000;
    for (;;)
    {
        u32 curtick = (u32)Platform::GetMSCount();
        if (curtick < starttick) break;
        int timeout = conntimeout - (int)(curtick - starttick);
        if (timeout < 0) break;
        if (enet_host_service(Host, &event, timeout) > 0)
        {
            if (conn == 0 && event.type == ENET_EVENT_TYPE_CONNECT)
            {
                enet_peer_throttle_configure(event.peer, ENET_PEER_PACKET_THROTTLE_INTERVAL,
                    ENET_PEER_PACKET_THROTTLE_ACCELERATION, 0);
                conn = 1;
            }
            else if (conn == 1 && event.type == ENET_EVENT_TYPE_RECEIVE)
            {
                u8* data = event.packet->data;
                if (event.channelID != Chan_Cmd || event.packet->dataLength != 21
                    || data[0] != Cmd_ClientInit)
                {
                    enet_packet_destroy(event.packet);
                    continue;
                }

                u32 magic = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                u32 version = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                if (magic != kLANMagic || version != kProtocolVersion
                    || data[10] < 2 || data[10] > 16 || data[9] == 0 || data[9] >= data[10]
                    || data[11] != (hle ? 1 : 0) || data[12] != WMServiceVersion || !ReadWMSession(data + 11))
                {
                    enet_packet_destroy(event.packet);
                    continue;
                }

                MaxPlayers = data[10];
                WMServiceSession = ReadWMSession(data + 11);

                // send player information
                MyPlayer.ID = data[9];
                u8 cmd[19+sizeof(Player)];
                cmd[0] = Cmd_PlayerInfo;
                cmd[1] = (u8)kLANMagic;
                cmd[2] = (u8)(kLANMagic >> 8);
                cmd[3] = (u8)(kLANMagic >> 16);
                cmd[4] = (u8)(kLANMagic >> 24);
                cmd[5] = (u8)kProtocolVersion;
                cmd[6] = (u8)(kProtocolVersion >> 8);
                cmd[7] = (u8)(kProtocolVersion >> 16);
                cmd[8] = (u8)(kProtocolVersion >> 24);
                WriteWMSession(cmd + 9, WMServiceHLE, WMServiceSession);
                memcpy(&cmd[19], &MyPlayer, sizeof(Player));
                ENetPacket* pkt = enet_packet_create(cmd, 19+sizeof(Player), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, Chan_Cmd, pkt);
                enet_packet_destroy(event.packet);

                conn = 2;
                break;
            }
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT)
            {
                conn = 0;
                break;
            }
        }
        else
            break;
    }

    if (conn != 2)
    {
        enet_peer_reset(peer);
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }

    HostAddress = addr.host;
    LastHostID = -1;
    LastHostPeer = nullptr;
    RemotePeers[0] = peer;
    WMConnections[0] = ++WMConnectionCounter;
    peer->data = &Players[0];

    Active = true;
    IsHost = false;
    SendWMServiceCapability(peer);
    return true;
}

void LAN::ClearReceiveQueue()
{
    for (auto* queue : {&RXDeferredQueue, &RXQueue})
    {
        while (!queue->empty())
        {
            ENetPacket* packet = queue->front();
            queue->pop();
            enet_packet_destroy(packet);
        }
    }
}

void LAN::EndSession()
{
    EndDiscovery();

    Active = false;

    ClearReceiveQueue();

    ResetWMService();
    WMServiceCapabilities = 0;
    WMServiceSession = 0;
    WMServiceTerminal = false;

    for (int i = 0; i < 16; i++)
    {
        if (i == MyPlayer.ID) continue;

        if (RemotePeers[i])
            enet_peer_disconnect(RemotePeers[i], 0);

        RemotePeers[i] = nullptr;
    }

    if (Host)
    {
        enet_host_flush(Host);
        enet_host_destroy(Host);
    }
    Host = nullptr;
    IsHost = false;
    NumPlayers = 0;
    MaxPlayers = 0;
    ConnectedBitmask = 0;
    FirmwareBootMask = 0;
    LastHostID = -1;
    LastHostPeer = nullptr;
    memset(Players, 0, sizeof(Players));
}


void LAN::ProcessDiscovery()
{
    if (DiscoverySocket == INVALID_SOCKET)
        return;

    u32 tick = (u32)Platform::GetMSCount();
    if ((tick - DiscoveryLastTick) < 1000)
        return;

    DiscoveryLastTick = tick;

    if (IsHost)
    {
        // advertise this LAN session over the network

        DiscoveryData beacon;
        memset(&beacon, 0, sizeof(beacon));
        beacon.Magic = kDiscoveryMagic;
        beacon.Version = kProtocolVersion;
        beacon.Tick = tick;
        snprintf(beacon.SessionName, 64, "%s's game", MyPlayer.Name);
        beacon.NumPlayers = NumPlayers;
        beacon.MaxPlayers = MaxPlayers;
        beacon.Status = 0; // TODO

        sockaddr_in_t saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(kDiscoveryPort);

        for (u32 address : BroadcastAddresses(DiscoverySocket))
        {
            saddr.sin_addr.s_addr = address;
            sendto(DiscoverySocket, (const char*)&beacon, sizeof(beacon), 0, (const sockaddr_t*)&saddr, sizeof(saddr));
        }
    }
    else
    {
        Platform::Mutex_Lock(DiscoveryMutex);

        // listen for LAN sessions

        fd_set fd;
        struct timeval tv;
        for (;;)
        {
            FD_ZERO(&fd); FD_SET(DiscoverySocket, &fd);
            tv.tv_sec = 0; tv.tv_usec = 0;
            if (select(DiscoverySocket+1, &fd, nullptr, nullptr, &tv) <= 0)
                break;

            DiscoveryData beacon;
            sockaddr_in_t raddr;
            socklen_t ralen = sizeof(raddr);

            int rlen = recvfrom(DiscoverySocket, (char*)&beacon, sizeof(beacon), 0, (sockaddr_t*)&raddr, &ralen);
            if (rlen < sizeof(beacon)) continue;
            if (beacon.Magic != kDiscoveryMagic) continue;
            if (beacon.Version != kProtocolVersion) continue;
            if (beacon.MaxPlayers < 2 || beacon.MaxPlayers > 16) continue;
            if (beacon.NumPlayers > beacon.MaxPlayers) continue;

            u32 key = ntohl(raddr.sin_addr.s_addr);

            if (DiscoveryList.find(key) != DiscoveryList.end())
            {
                if (beacon.Tick <= DiscoveryList[key].Tick)
                    continue;
            }

            if (DiscoveryList.size() >= 16 && DiscoveryList.find(key) == DiscoveryList.end()) continue;

            beacon.Magic = tick;
            beacon.SessionName[63] = '\0';
            DiscoveryList[key] = beacon;
        }

        // cleanup: remove hosts that haven't given a sign of life in the last 5 seconds

        std::vector<u32> deletelist;

        for (const auto& [key, data] : DiscoveryList)
        {
            u32 age = tick - data.Magic;
            if (age < 5000) continue;

            deletelist.push_back(key);
        }

        for (const auto& key : deletelist)
        {
            DiscoveryList.erase(key);
        }

        Platform::Mutex_Unlock(DiscoveryMutex);
    }
}

void LAN::HostUpdatePlayerList()
{
    u8 cmd[2+sizeof(Players)];
    cmd[0] = Cmd_PlayerList;
    cmd[1] = (u8)NumPlayers;
    memcpy(&cmd[2], Players, sizeof(Players));
    ENetPacket* pkt = enet_packet_create(cmd, 2+sizeof(Players), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Cmd, pkt);
    SendFirmwareBoot();
    if (ConnectedBitmask & (1 << MyPlayer.ID))
    {
        u8 connected = Cmd_PlayerConnect;
        ENetPacket* state = enet_packet_create(&connected, 1, ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(Host, Chan_Cmd, state);
    }
}

void LAN::ClientUpdatePlayerList()
{
}

void LAN::ProcessHostEvent(ENetEvent& event)
{
    switch (event.type)
    {
    case ENET_EVENT_TYPE_CONNECT:
        {
            if ((NumPlayers >= MaxPlayers) || (NumPlayers >= 16))
            {
                // game is full, reject connection
                enet_peer_disconnect(event.peer, 0);
                break;
            }

            // client connected; assign player number

            int id;
            for (id = 0; id < 16; id++)
            {
                if (Players[id].Status == Player_None) break;
            }

            if (id < 16)
            {
                u8 cmd[21];
                cmd[0] = Cmd_ClientInit;
                cmd[1] = (u8)kLANMagic;
                cmd[2] = (u8)(kLANMagic >> 8);
                cmd[3] = (u8)(kLANMagic >> 16);
                cmd[4] = (u8)(kLANMagic >> 24);
                cmd[5] = (u8)kProtocolVersion;
                cmd[6] = (u8)(kProtocolVersion >> 8);
                cmd[7] = (u8)(kProtocolVersion >> 16);
                cmd[8] = (u8)(kProtocolVersion >> 24);
                cmd[9] = (u8)id;
                cmd[10] = MaxPlayers;
                WriteWMSession(cmd + 11, WMServiceHLE, WMServiceSession);
                ENetPacket* pkt = enet_packet_create(cmd, 21, ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, Chan_Cmd, pkt);

                Platform::Mutex_Lock(PlayersMutex);

                Players[id].ID = id;
                Players[id].Status = Player_Connecting;
                Players[id].Address = event.peer->address.host;
                Players[id].Port = event.peer->address.port;
                event.peer->data = &Players[id];
                NumPlayers++;

                Platform::Mutex_Unlock(PlayersMutex);

                RemotePeers[id] = event.peer;
                WMConnections[id] = ++WMConnectionCounter;
                WMServiceCapabilities &= ~(1u << id);
            }
            else
            {
                // ???
                enet_peer_disconnect(event.peer, 0);
            }
        }
        break;

    case ENET_EVENT_TYPE_DISCONNECT:
        {
            Player* player = (Player*)event.peer->data;
            if (!player) break;

            ConnectedBitmask &= ~(1 << player->ID);
            FirmwareBootMask &= ~(1 << player->ID);
            WMServiceCapabilities &= ~(1u << player->ID);

            int id = player->ID;
            RemotePeers[id] = nullptr;

            player->ID = 0;
            player->Status = Player_None;
            NumPlayers--;

            // broadcast updated player list
            HostUpdatePlayerList();
        }
        break;

    case ENET_EVENT_TYPE_RECEIVE:
        {
            if (event.packet->dataLength < 1) { enet_packet_destroy(event.packet); break; }

            u8* data = (u8*)event.packet->data;
            switch (data[0])
            {
            case Cmd_PlayerInfo: // client sending player info
                {
                    if (event.packet->dataLength != (19+sizeof(Player))) break;

                    u32 magic = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                    u32 version = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                    if ((magic != kLANMagic) || (version != kProtocolVersion)
                        || data[9] != (WMServiceHLE ? 1 : 0) || data[10] != WMServiceVersion
                        || ReadWMSession(data + 9) != WMServiceSession)
                    {
                        enet_peer_disconnect(event.peer, 0);
                        break;
                    }

                    Player player;
                    memcpy(&player, &data[19], sizeof(Player));
                    player.Name[31] = '\0';
                    player.IsLocalPlayer = false;
                    player.Ping = 0;

                    Player* hostside = (Player*)event.peer->data;
                    if (!hostside || player.ID != hostside->ID)
                    {
                        enet_peer_disconnect(event.peer, 0);
                        break;
                    }

                    Platform::Mutex_Lock(PlayersMutex);

                    player.Status = Player_Client;
                    player.Address = event.peer->address.host;
                    player.Port = event.peer->address.port;
                    memcpy(hostside, &player, sizeof(Player));

                    Platform::Mutex_Unlock(PlayersMutex);

                    // broadcast updated player list
                    HostUpdatePlayerList();
                    SendWMServiceCapability(event.peer);
                }
                break;

            case Cmd_PlayerConnect: // player connected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask |= (1 << player->ID);
                }
                break;

            case Cmd_PlayerDisconnect: // player disconnected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask &= ~(1 << player->ID);
                }
                break;
            }

            enet_packet_destroy(event.packet);
        }
        break;
    case ENET_EVENT_TYPE_NONE:
        break;
    }
}

void LAN::ProcessClientEvent(ENetEvent& event)
{
    switch (event.type)
    {
    case ENET_EVENT_TYPE_CONNECT:
        {
            // another client is establishing a direct connection to us

            int playerid = -1;
            for (int i = 0; i < 16; i++)
            {
                Player* player = &Players[i];
                if (i == MyPlayer.ID) continue;
                if (player->Status != Player_Client) continue;

                if (player->Address == event.peer->address.host && player->Port == event.peer->address.port)
                {
                    playerid = i;
                    break;
                }
            }

            if (playerid < 0 || (RemotePeers[playerid] && RemotePeers[playerid] != event.peer))
            {
                enet_peer_disconnect(event.peer, 0);
                break;
            }

            RemotePeers[playerid] = event.peer;
            WMConnections[playerid] = ++WMConnectionCounter;
            event.peer->data = &Players[playerid];
            WMServiceCapabilities &= ~(1u << playerid);
            SendWMServiceCapability(event.peer);
            SendFirmwareBoot();
            if (ConnectedBitmask & (1u << MyPlayer.ID))
            {
                u8 ready = Cmd_PlayerConnect;
                ENetPacket* packet = enet_packet_create(&ready, 1, ENET_PACKET_FLAG_RELIABLE);
                if (packet) enet_peer_send(event.peer, Chan_Cmd, packet);
            }
        }
        break;

    case ENET_EVENT_TYPE_DISCONNECT:
        {
            Player* player = (Player*)event.peer->data;
            if (!player) break;

            ConnectedBitmask &= ~(1 << player->ID);
            FirmwareBootMask &= ~(1 << player->ID);
            WMServiceCapabilities &= ~(1u << player->ID);

            int id = player->ID;
            RemotePeers[id] = nullptr;

            Platform::Mutex_Lock(PlayersMutex);
            player->Status = Player_Disconnected;
            Platform::Mutex_Unlock(PlayersMutex);

            ClientUpdatePlayerList();
        }
        break;

    case ENET_EVENT_TYPE_RECEIVE:
        {
            if (event.packet->dataLength < 1) { enet_packet_destroy(event.packet); break; }

            u8* data = (u8*)event.packet->data;
            switch (data[0])
            {
            case Cmd_PlayerList: // host sending player list
                {
                    if (event.packet->dataLength != (2+sizeof(Players))) break;
                    if (data[1] > 16 || event.peer != RemotePeers[0]) break;

                    Player incoming[16];
                    memcpy(incoming, &data[2], sizeof(incoming));
                    bool valid = true;
                    for (int i = 0; i < 16; i++)
                    {
                        if (incoming[i].Status < Player_None || incoming[i].Status > Player_Disconnected
                            || (incoming[i].Status != Player_None && (incoming[i].ID != i || !incoming[i].Port))) valid = false;
                        incoming[i].IsLocalPlayer = false;
                    }
                    if (!valid) break;

                    Platform::Mutex_Lock(PlayersMutex);

                    NumPlayers = data[1];
                    memcpy(Players, incoming, sizeof(Players));
                    for (int i = 0; i < 16; i++)
                    {
                        Players[i].Name[31] = '\0';
                    }

                    Platform::Mutex_Unlock(PlayersMutex);

                    // establish connections to any new clients
                    for (int i = 0; i < 16; i++)
                    {
                        Player* player = &Players[i];
                        if (i == 0 || i == MyPlayer.ID) continue;
                        if (RemotePeers[i] && (player->Status != Player_Client
                            || RemotePeers[i]->address.host != player->Address
                            || RemotePeers[i]->address.port != player->Port))
                        {
                            ConnectedBitmask &= ~(1u << i);
                            FirmwareBootMask &= ~(1u << i);
                            WMServiceCapabilities &= ~(1u << i);
                            enet_peer_disconnect_now(RemotePeers[i], 0);
                            RemotePeers[i] = nullptr;
                        }
                        if (player->Status != Player_Client || i > MyPlayer.ID) continue;

                        if (!RemotePeers[i])
                        {
                            ENetAddress peeraddr;
                            peeraddr.host = player->Address;
                            peeraddr.port = player->Port;
                            ENetPeer* peer = enet_host_connect(Host, &peeraddr, 2, 0);
                            if (!peer)
                            {
                                // TODO deal with this
                                continue;
                            }
                            RemotePeers[i] = peer;
                            WMConnections[i] = ++WMConnectionCounter;
                            peer->data = player;
                        }
                    }
                }
                break;

            case Cmd_PlayerConnect: // player connected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask |= (1 << player->ID);
                }
                break;

            case Cmd_PlayerDisconnect: // player disconnected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask &= ~(1 << player->ID);
                }
                break;
            }

            enet_packet_destroy(event.packet);
        }
        break;
    case ENET_EVENT_TYPE_NONE:
        break;
    }
}

void LAN::ProcessEvent(ENetEvent& event)
{
    if (event.type == ENET_EVENT_TYPE_RECEIVE && event.channelID == Chan_Cmd
        && event.packet->dataLength > 0
        && (event.packet->data[0] == Cmd_WMServiceCapability || event.packet->data[0] == Cmd_WMServiceData))
    {
        ProcessWMServiceEvent(event);
        enet_packet_destroy(event.packet);
        return;
    }
    if (event.type == ENET_EVENT_TYPE_CONNECT)
        enet_peer_throttle_configure(event.peer, ENET_PEER_PACKET_THROTTLE_INTERVAL,
            ENET_PEER_PACKET_THROTTLE_ACCELERATION, 0);
    if (IsHost)
        ProcessHostEvent(event);
    else
        ProcessClientEvent(event);
}

// 0 = per-frame processing of events and eventual misc. frame
// 1 = checking if a misc. frame has arrived
// 2 = waiting for a MP frame
void LAN::ProcessLAN(int type, const ReceiveWait* wait)
{
    if (!Host) return;

    u32 time_last = (u32)Platform::GetMSCount();

    while (!RXQueue.empty())
    {
        if (type == 0) break;
        ENetPacket* packet = RXQueue.front();
        const auto* header = reinterpret_cast<const MPPacketHeader*>(packet->data);
        if (type == 2 || header->Type == 0) return;
        RXQueue.pop();
        enet_packet_destroy(packet);
    }

    if (wait)
    {
        if (type == 2) wait->Callback(wait->Context, MPRecvTimeout);
        return;
    }

    int timeout = (type == 2) ? MPRecvTimeout : 0;
    time_last = (u32)Platform::GetMSCount();

    ENetEvent event;
    while (enet_host_service(Host, &event, timeout) > 0)
    {
        if (event.type == ENET_EVENT_TYPE_RECEIVE && event.channelID == Chan_MP)
        {
            MPPacketHeader* header = (MPPacketHeader*)&event.packet->data[0];

            bool good = true;
            if (event.packet->dataLength < sizeof(MPPacketHeader))
                good = false;
            else if (header->Magic != 0x4946494E)
                good = false;
            else if (header->SenderID == MyPlayer.ID)
                good = false;
            else if (header->SenderID >= 16 || !event.peer->data
                || header->SenderID != (u32)((Player*)event.peer->data)->ID)
                good = false;
            else if (header->Length != event.packet->dataLength - sizeof(MPPacketHeader)
                || header->Length > 2048 || (header->Length && header->Length < 12))
                good = false;
            else if ((header->Type & 0xFFFF) > 5)
                good = false;
            else if (header->Type == 5 && (header->Length || header->Timestamp > 1
                || RemotePeers[header->SenderID] != event.peer))
                good = false;
            else if (header->Type == 4 && header->Length)
                good = false;
            else if (header->Type == 4 && (event.packet->flags & ENET_PACKET_FLAG_UNSEQUENCED))
                good = false;
            else if ((header->Type & 0xFFFF) != 2
                && header->Type != 4
                && !(event.packet->flags & ENET_PACKET_FLAG_RELIABLE))
                good = false;
            else if ((header->Type & 0xFFFF) == 2 && header->Length
                && ((header->Type >> 16) < 1 || (header->Type >> 16) > 15 || header->Length > 1024))
                good = false;
            else if ((header->Type & 0xFFFF) != 2 && (header->Type >> 16))
                good = false;

            if (!good)
            {
                enet_packet_destroy(event.packet);
            }
            else
            {
                if (header->Type == 5)
                {
                    const u16 bit = 1u << header->SenderID;
                    if (header->Timestamp) FirmwareBootMask |= bit;
                    else FirmwareBootMask &= ~bit;
                    enet_packet_destroy(event.packet);
                    continue;
                }
                if (WMServiceHLE)
                {
                    if (RemotePeers[header->SenderID] != event.peer
                        || !(GetWMServiceInfo().Peers & (1u << header->SenderID)))
                    {
                        enet_packet_destroy(event.packet);
                        continue;
                    }
                    WMServiceTerminal = true;
                    Platform::Log(Platform::LogLevel::Error, "LAN HLE rejected raw receive type=%u sender=%u session=%llu\n",
                        header->Type, header->SenderID, static_cast<unsigned long long>(WMServiceSession));
                    enet_packet_destroy(event.packet);
                    continue;
                }
                event.packet->userData = event.peer;
                if (!(ConnectedBitmask & (1u << MyPlayer.ID)))
                {
                    enet_packet_destroy(event.packet);
                    continue;
                }
                if (RXQueue.size() + RXDeferredQueue.size() >= 256)
                {
                    enet_packet_destroy(event.packet);
                    Platform::Log(Platform::LogLevel::Warn, "LAN receive queue exhausted\n");
                    End(0);
                    return;
                }
                RXQueue.push(event.packet);

                // return now -- if we are receiving MP frames, if we keep going
                // we'll consume too many even if we have no timeout set
                return;
            }
        }
        else
        {
            const size_t queuedWM = WMServiceReceiveQueue.size();
            ProcessEvent(event);
            if (type != 2 && WMServiceReceiveQueue.size() != queuedWM) return;
        }

        if (type == 2)
        {
            u32 time = (u32)Platform::GetMSCount();
            if (time < time_last) return;
            timeout -= (int)(time - time_last);
            if (timeout <= 0) return;
            time_last = time;
        }
    }
}

void LAN::Process()
{
    if (!Active) return;

    ProcessDiscovery();
    ProcessLAN(0);

    FrameCount++;
    if (FrameCount >= 60)
    {
        FrameCount = 0;

        Platform::Mutex_Lock(PlayersMutex);

        for (int i = 0; i < 16; i++)
        {
            if (Players[i].Status == Player_None) continue;
            if (i == MyPlayer.ID) continue;
            if (!RemotePeers[i]) continue;

            Players[i].Ping = RemotePeers[i]->roundTripTime;
        }

        Platform::Mutex_Unlock(PlayersMutex);
    }
}

void LAN::SendWMServiceCapability(ENetPeer* peer)
{
    u8 data[11]{};
    data[0] = Cmd_WMServiceCapability;
    WriteWMSession(data + 1, WMServiceHLE, WMServiceSession);
    ENetPacket* packet = enet_packet_create(data, sizeof(data), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) return;
    if (enet_peer_send(peer, Chan_Cmd, packet) != 0) enet_packet_destroy(packet);
}

void LAN::ProcessWMServiceEvent(const ENetEvent& event)
{
    const auto* player = static_cast<const Player*>(event.peer->data);
    if (!player || player->ID < 0 || player->ID >= 16 || player->ID == MyPlayer.ID
        || RemotePeers[player->ID] != event.peer
        || (player->Status != Player_Client && player->Status != Player_Host)
        || !(event.packet->flags & ENET_PACKET_FLAG_RELIABLE)) return;
    const u8* data = event.packet->data;
    const size_t size = event.packet->dataLength;
    const u16 bit = 1u << player->ID;
    if (data[0] == Cmd_WMServiceCapability)
    {
        if (size != 11 || data[1] != (WMServiceHLE ? 1 : 0) || data[2] != WMServiceVersion
            || ReadWMSession(data + 1) != WMServiceSession)
        {
            WMServiceCapabilities &= ~bit;
            enet_peer_disconnect(event.peer, 0);
            return;
        }
        WMServiceCapabilities |= bit;
        return;
    }
    if (!WMServiceHLE || WMServiceTerminal || size < 8 || size > 8 + 2048 || data[1] != WMServiceVersion || data[2] != player->ID
        || data[3] || data[6] || data[7] || !(WMServiceCapabilities & bit)) return;
    const u16 length = data[4] | (u16(data[5]) << 8);
    if (length != size - 8) return;
    if (WMServiceReceiveFailed) return;
    if (WMServiceReceiveQueue.size() >= 256)
    {
        WMServiceReceiveFailed = true;
        Platform::Log(Platform::LogLevel::Warn, "LAN WM service receive queue exhausted\n");
        return;
    }
    WMServicePacket packet;
    packet.SenderID = static_cast<u8>(player->ID);
    packet.Connection = WMConnections[player->ID];
    packet.Length = length;
    if (length) memcpy(packet.Data.data(), data + 8, length);
    WMServiceReceiveQueue.push(std::move(packet));
}

bool LAN::SupportsWMService() const
{
    if (!Host || !Active || !WMServiceHLE || NumPlayers < 2 || WMServiceReceiveFailed || WMServiceTerminal) return false;
    bool found = false;
    for (int i = 0; i < 16; i++)
    {
        if (i == MyPlayer.ID) continue;
        if (Players[i].Status == Player_Connecting) return false;
        if (Players[i].Status != Player_Client && Players[i].Status != Player_Host) continue;
        if (!RemotePeers[i] || RemotePeers[i]->state != ENET_PEER_STATE_CONNECTED
            || !(WMServiceCapabilities & (1u << i))) return false;
        found = true;
    }
    return found;
}

WMServiceInfo LAN::GetWMServiceInfo() const
{
    if (!Host || !Active || !WMServiceHLE || !WMServiceSession) return {};
    WMServiceInfo result;
    result.Session = WMServiceSession;
    result.LocalID = MyPlayer.ID;
    result.Peers = GetWMServicePeerMask() & WMServiceCapabilities;
    result.Terminal = WMServiceTerminal;
    return result;
}

int LAN::GetWMServiceLocalID() const
{
    return Host && Active ? MyPlayer.ID : -1;
}

u16 LAN::GetWMServicePeerMask() const
{
    if (!Host || !Active) return 0;
    u16 mask = 0;
    for (int i = 0; i < 16; i++)
    {
        if (i != MyPlayer.ID && RemotePeers[i] && RemotePeers[i]->state == ENET_PEER_STATE_CONNECTED
            && (Players[i].Status == Player_Client || Players[i].Status == Player_Host))
            mask |= 1u << i;
    }
    return mask;
}

u16 LAN::SendWMService(u16 peers, const u8* data, u16 length)
{
    if (length > 2048 || (length && !data) || !Host || !Active || !WMServiceHLE
        || WMServiceReceiveFailed || WMServiceTerminal) return 0;
    peers &= GetWMServiceInfo().Peers;
    u16 queued = 0;
    for (int i = 0; i < 16; i++)
    {
        if (!(peers & (1u << i))) continue;
        ENetPacket* packet = enet_packet_create(nullptr, 8 + length, ENET_PACKET_FLAG_RELIABLE);
        if (!packet) continue;
        const u8 header[] = {Cmd_WMServiceData, WMServiceVersion, static_cast<u8>(MyPlayer.ID), 0,
                             static_cast<u8>(length), static_cast<u8>(length >> 8), 0, 0};
        memcpy(packet->data, header, sizeof(header));
        if (length) memcpy(packet->data + sizeof(header), data, length);
        if (enet_peer_send(RemotePeers[i], Chan_Cmd, packet) != 0)
        {
            enet_packet_destroy(packet);
            continue;
        }
        queued |= 1u << i;
    }
    if (queued) Flush();
    return queued;
}

int LAN::ReceiveWMService(WMServicePacket& packet)
{
    if (WMServiceReceiveFailed) return -1;
    if (WMServiceReceiveQueue.empty()) ProcessLAN(0);
    if (WMServiceReceiveFailed) return -1;
    while (!WMServiceReceiveQueue.empty())
    {
        packet = std::move(WMServiceReceiveQueue.front());
        WMServiceReceiveQueue.pop();
        if (packet.SenderID < 16 && packet.Connection == WMConnections[packet.SenderID]
            && (GetWMServiceInfo().Peers & (1u << packet.SenderID))) return 1;
    }
    return 0;
}

void LAN::ResetWMService()
{
    std::queue<WMServicePacket>().swap(WMServiceReceiveQueue);
    WMServiceReceiveFailed = false;
}


void LAN::SendFirmwareBoot()
{
    SendPacketGeneric(5, nullptr, 0, (FirmwareBootMask >> MyPlayer.ID) & 1);
}

void LAN::SetFirmwareBoot(bool firmware)
{
    const u16 bit = 1u << MyPlayer.ID;
    if (bool(FirmwareBootMask & bit) == firmware) return;
    if (firmware) FirmwareBootMask |= bit;
    else FirmwareBootMask &= ~bit;
    SendFirmwareBoot();
}

void LAN::Begin(int inst)
{
    if (!Host) return;

    ClearReceiveQueue();
    ConnectedBitmask |= (1 << MyPlayer.ID);
    LastHostID = -1;
    LastHostPeer = nullptr;

    u8 cmd = Cmd_PlayerConnect;
    ENetPacket* pkt = enet_packet_create(&cmd, 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Cmd, pkt);
}

void LAN::End(int inst)
{
    if (!Host) return;

    ConnectedBitmask &= ~(1 << MyPlayer.ID);
    ClearReceiveQueue();

    u8 cmd = Cmd_PlayerDisconnect;
    ENetPacket* pkt = enet_packet_create(&cmd, 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Cmd, pkt);
}


int LAN::SendPacketGeneric(u32 type, u8* packet, int len, u64 timestamp)
{
    if (!Host || len < 0 || len > 2048 || (len && !packet)) return 0;
    if (WMServiceHLE && type != 5)
    {
        WMServiceTerminal = true;
        Platform::Log(Platform::LogLevel::Error, "LAN HLE rejected raw send type=%u session=%llu\n",
            type, static_cast<unsigned long long>(WMServiceSession));
        return 0;
    }

    // TODO make the reliable part optional?
    //u32 flags = ENET_PACKET_FLAG_RELIABLE;
    u32 flags = (type & 0xFFFF) == 2 ? ENET_PACKET_FLAG_UNSEQUENCED
        : type == 4 ? 0 : ENET_PACKET_FLAG_RELIABLE;

    ENetPacket* enetpacket = enet_packet_create(nullptr, sizeof(MPPacketHeader)+len, flags);
    if (!enetpacket) return 0;

    MPPacketHeader pktheader;
    pktheader.Magic = 0x4946494E;
    pktheader.SenderID = MyPlayer.ID;
    pktheader.Type = type;
    pktheader.Length = len;
    pktheader.Timestamp = timestamp;
    memcpy(&enetpacket->data[0], &pktheader, sizeof(MPPacketHeader));
    if (len)
        memcpy(&enetpacket->data[sizeof(MPPacketHeader)], packet, len);

    if (((type & 0xFFFF) == 2) && LastHostPeer)
        enet_peer_send(LastHostPeer, Chan_MP, enetpacket);
    else
        enet_host_broadcast(Host, Chan_MP, enetpacket);
    if (FlushOnSend) Flush();

    return type == 4 ? 1 : len;
}

int LAN::RecvPacketGeneric(u8* packet, bool block, u64* timestamp, const ReceiveWait* wait)
{
    if (!Host) return 0;

    if (RXDeferredQueue.empty()) ProcessLAN(block ? 2 : 1, wait);
    auto& queue = RXDeferredQueue.empty() ? RXQueue : RXDeferredQueue;
    if (queue.empty()) return 0;

    ENetPacket* enetpacket = queue.front();
    queue.pop();
    MPPacketHeader* header = (MPPacketHeader*)&enetpacket->data[0];

    u32 len = header->Length;
    if (header->Type == 4)
    {
        const bool progress = block && header->SenderID == static_cast<u32>(LastHostID)
            && enetpacket->userData == LastHostPeer;
        if (progress && timestamp) *timestamp = header->Timestamp;
        enet_packet_destroy(enetpacket);
        return progress ? -2 : 0;
    }
    if (len)
    {
        if (len > 2048) len = 2048;

        memcpy(packet, &enetpacket->data[sizeof(MPPacketHeader)], len);

        if (header->Type == 1)
        {
            LastHostID = header->SenderID;
            LastHostPeer = (ENetPeer*)enetpacket->userData;
        }
    }

    if (timestamp) *timestamp = header->Timestamp;
    enet_packet_destroy(enetpacket);
    return len;
}


int LAN::SendPacket(int inst, u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(0, packet, len, timestamp);
}

int LAN::RecvPacket(int inst, u8* packet, u64* timestamp)
{
    return RecvPacket(inst, packet, timestamp, nullptr);
}

int LAN::RecvPacket(int inst, u8* packet, u64* timestamp, const ReceiveWait* wait)
{
    return RecvPacketGeneric(packet, false, timestamp, wait);
}


int LAN::SendCmd(int inst, u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(1, packet, len, timestamp);
}

int LAN::SendReply(int inst, u8* packet, int len, u64 timestamp, u16 aid)
{
    return SendPacketGeneric(2 | (aid<<16), packet, len, timestamp);
}

int LAN::SendAck(int inst, u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(3, packet, len, timestamp);
}

int LAN::SendProgress(int inst, u64 timestamp)
{
    return SendPacketGeneric(4, nullptr, 0, timestamp);
}

int LAN::RecvHostPacket(int inst, u8* packet, u64* timestamp)
{
    return RecvHostPacket(inst, packet, timestamp, nullptr);
}

int LAN::RecvHostPacket(int inst, u8* packet, u64* timestamp, const ReceiveWait* wait)
{
    if (!Host || !(ConnectedBitmask & (1 << MyPlayer.ID))) return -1;
    if (LastHostID != -1)
    {
        // check if the host is still connected

        if (!(ConnectedBitmask & (1<<LastHostID)))
            return -1;
    }

    return RecvPacketGeneric(packet, true, timestamp, wait);
}

u16 LAN::RecvReplies(int inst, u8* packets, u64 timestamp, u16 aidmask)
{
    return RecvReplies(inst, packets, timestamp, aidmask, nullptr);
}

u16 LAN::RecvReplies(int inst, u8* packets, u64 timestamp, u16 aidmask, const ReceiveWait* wait)
{
    if (!Host) return 0;

    u16 ret = 0;
    u16 myinstmask = 1 << MyPlayer.ID;

    if ((myinstmask & ConnectedBitmask) == ConnectedBitmask)
        return 0;

    for (;;)
    {
        ProcessLAN(2, wait);
        if (RXQueue.empty())
        {
            // no more replies available
            return ret;
        }

        ENetPacket* enetpacket = RXQueue.front();
        RXQueue.pop();
        MPPacketHeader* header = (MPPacketHeader*)&enetpacket->data[0];
        if (header->Type == 0)
        {
            RXDeferredQueue.push(enetpacket);
            continue;
        }
        bool good = true;
        if ((header->Type & 0xFFFF) != 2)
            good = false;
        else if (header->Timestamp < (timestamp > 32 ? timestamp - 32 : 0))
            good = false;

        if (good)
        {
            u32 len = header->Length;
            if (len)
            {
                if (len > 1024) len = 1024;

                u32 aid = header->Type >> 16;
                memcpy(&packets[(aid-1)*1024], &enetpacket->data[sizeof(MPPacketHeader)], len);

                ret |= (1<<aid);
            }

            myinstmask |= (1<<header->SenderID);
            if (((myinstmask & ConnectedBitmask) == ConnectedBitmask) ||
                ((ret & aidmask) == aidmask))
            {
                // all the clients have sent their reply
                enet_packet_destroy(enetpacket);
                return ret;
            }
        }

        enet_packet_destroy(enetpacket);
    }
}

}
