#include "headers.h"

#if _WIN32
#include <WinSock2.h>
#else
#include <sys/select.h>
#endif

#include "core/socket.h"
#include "core/timer.h"
#include "core/window.h"
#include "core/log.h"
#include "core/circbuf.h"

#include "main.h"
#include "net.h"
#include "player.h"
#include "settings.h"
#include "projectile.h"

//#define SERVER_PRINT_SIMPLE 1
//#define SERVER_PRINT_VERBOSE 1

#define GAME_ID 0xC68BB822
#define PORT 27001

#define MAXIMUM_RTT 1.0f

#define DEFAULT_TIMEOUT 1.0f // seconds
#define PING_PERIOD 3.0f
#define DISCONNECTION_TIMEOUT 7.0f // seconds
#define INPUT_QUEUE_MAX 16

typedef struct
{
    int socket;
    uint16_t local_latest_packet_id;
    uint16_t remote_latest_packet_id;
} NodeInfo;

// Info server stores about a client
typedef struct
{
    int client_id;
    Address address;
    ConnectionState state;
    uint16_t remote_latest_packet_id;
    double  time_of_latest_packet;
    uint8_t client_salt[8];
    uint8_t server_salt[8];
    uint8_t xor_salts[8];
    PlayerNetState player_state;
    ConnectionRejectionReason last_reject_reason;
    PacketError last_packet_error;
    Packet prior_state_pkt;
    NetPlayerInput net_player_inputs[INPUT_QUEUE_MAX];
    int input_count;
} ClientInfo;

struct
{
    Address address;
    NodeInfo info;
    ClientInfo clients[MAX_CLIENTS];
    int num_clients;
} server = {0};

// ---

#define IMAX_BITS(m) ((m)/((m)%255+1) / 255%255*8 + 7-86/((m)%255+12))
#define RAND_MAX_WIDTH IMAX_BITS(RAND_MAX)

// ---

//static PlayerNetState net_player_states[MAX_CLIENTS];
static NetPlayerInput net_player_inputs[INPUT_QUEUE_MAX]; // shared
static int input_count = 0;
static int inputs_per_packet = 1.0; //(TARGET_FPS/TICK_RATE);

static inline void pack_u8(Packet* pkt, uint8_t d);
static inline void pack_u16(Packet* pkt, uint16_t d);
static inline void pack_u32(Packet* pkt, uint32_t d);
static inline void pack_u64(Packet* pkt, uint64_t d);
static inline void pack_float(Packet* pkt, float d);
static inline void pack_bytes(Packet* pkt, uint8_t* d, uint8_t len);
static inline void pack_string(Packet* pkt, char* s, uint8_t max_len);
static inline void pack_vec2(Packet* pkt, Vector2f d);

static inline uint8_t  unpack_u8(Packet* pkt, int* offset);
static inline uint16_t unpack_u16(Packet* pkt, int* offset);
static inline uint32_t unpack_u32(Packet* pkt, int* offset);
static inline uint64_t unpack_u64(Packet* pkt, int* offset);
static inline float    unpack_float(Packet* pkt, int* offset);
static inline void unpack_bytes(Packet* pkt, uint8_t* d, int len, int* offset);
static inline uint8_t unpack_string(Packet* pkt, char* s, int maxlen, int* offset);
static inline Vector2f unpack_vec2(Packet* pkt, int* offset);

static uint64_t rand64(void)
{
    uint64_t r = 0;
    for (int i = 0; i < 64; i += RAND_MAX_WIDTH) {
        r <<= RAND_MAX_WIDTH;
        r ^= (unsigned) rand();
    }
    return r;
}

static Timer server_timer = {0};

static inline int get_packet_size(Packet* pkt)
{
    return (sizeof(pkt->hdr) + pkt->data_len + sizeof(pkt->data_len));
}

static inline bool is_packet_id_greater(uint16_t id, uint16_t cmp)
{
    return ((id >= cmp) && (id - cmp <= 32768)) || 
           ((id <= cmp) && (cmp - id  > 32768));
}

static char* packet_type_to_str(PacketType type)
{
    switch(type)
    {
        case PACKET_TYPE_INIT: return "INIT";
        case PACKET_TYPE_CONNECT_REQUEST: return "CONNECT REQUEST";
        case PACKET_TYPE_CONNECT_CHALLENGE: return "CONNECT CHALLENGE";
        case PACKET_TYPE_CONNECT_CHALLENGE_RESP: return "CONNECT CHALLENGE RESP";
        case PACKET_TYPE_CONNECT_ACCEPTED: return "CONNECT ACCEPTED";
        case PACKET_TYPE_CONNECT_REJECTED: return "CONNECT REJECTED";
        case PACKET_TYPE_DISCONNECT: return "DISCONNECT";
        case PACKET_TYPE_PING: return "PING";
        case PACKET_TYPE_INPUT: return "INPUT";
        case PACKET_TYPE_SETTINGS: return "SETTINGS";
        case PACKET_TYPE_STATE: return "STATE";
        case PACKET_TYPE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static char* connect_reject_reason_to_str(ConnectionRejectionReason reason)
{
    switch(reason)
    {
        case CONNECT_REJECT_REASON_SERVER_FULL: return "SERVER FULL";
        case CONNECT_REJECT_REASON_INVALID_PACKET: return "INVALID PACKET FORMAT";
        case CONNECT_REJECT_REASON_FAILED_CHALLENGE: return "FAILED CHALLENGE";
        default: return "UNKNOWN";
    }
}

static void print_salt(uint8_t* salt)
{
    LOGN("[SALT] %02X %02X %02X %02X %02X %02X %02X %02X",
            salt[0], salt[1], salt[2], salt[3],
            salt[4], salt[5], salt[6], salt[7]
    );
}

static void store_xor_salts(uint8_t* cli_salt, uint8_t* svr_salt, uint8_t* xor_salts)
{
    for(int i = 0; i < 8; ++i)
    {
        xor_salts[i] = (cli_salt[i] ^ svr_salt[i]);
    }
}

static void print_address(Address* addr)
{
    LOGN("[ADDR] %u.%u.%u.%u:%u",addr->a,addr->b,addr->c,addr->d,addr->port);
}

static void print_packet(Packet* pkt)
{
    LOGN("Game ID:      0x%08x",pkt->hdr.game_id);
    LOGN("Packet ID:    %u",pkt->hdr.id);
    LOGN("Packet Type:  %s (%02X)",packet_type_to_str(pkt->hdr.type),pkt->hdr.type);
    LOGN("Data (%u):", pkt->data_len);

    char data[3*16+5] = {0};
    char byte[4] = {0};
    for(int i = 0; i < MIN(16,pkt->data_len); ++i)
    {
        sprintf(byte,"%02X ",pkt->data[i]);
        memcpy(data+(3*i), byte,3);
    }

    if(pkt->data_len <= 16)
    {
        LOGN("%s", data);
    }
    else
    {
        LOGN("%s ...", data);
    }
}

static void print_packet_simple(Packet* pkt, const char* hdr)
{
    LOGN("[%s][ID: %u] %s (%u B)",hdr, pkt->hdr.id, packet_type_to_str(pkt->hdr.type), pkt->data_len);
}

static bool has_data_waiting(int socket)
{

    fd_set readfds;

    //clear the socket set  
    FD_ZERO(&readfds);
    
    //add client socket to set  
    FD_SET(socket, &readfds);

    int activity;

    struct timeval tv = {0};
    activity = select(socket + 1 , &readfds , NULL , NULL , &tv);

    if ((activity < 0) && (errno!=EINTR))
    {
        perror("select error");
        return false;
    }

    bool has_data = FD_ISSET(socket , &readfds);
    return has_data;
}

static int net_send(NodeInfo* node_info, Address* to, Packet* pkt)
{
    int pkt_len = get_packet_size(pkt);
    int sent_bytes = socket_sendto(node_info->socket, to, (uint8_t*)pkt, pkt_len);

#if SERVER_PRINT_SIMPLE==1
    print_packet_simple(pkt,"SEND");
#elif SERVER_PRINT_VERBOSE==1
    LOGN("[SENT] Packet %d (%u B)",pkt->hdr.id,sent_bytes);
    print_packet(pkt);
#endif

    node_info->local_latest_packet_id++;

    return sent_bytes;
}

static int net_recv(NodeInfo* node_info, Address* from, Packet* pkt)
{
    int recv_bytes = socket_recvfrom(node_info->socket, from, (uint8_t*)pkt);

#if SERVER_PRINT_SIMPLE
    print_packet_simple(pkt,"RECV");
#elif SERVER_PRINT_VERBOSE
    LOGN("[RECV] Packet %d (%u B)",pkt->hdr.id,recv_bytes);
    print_address(from);
    print_packet(pkt);
#endif

    return recv_bytes;
}

static bool validate_packet_format(Packet* pkt)
{
    if(pkt->hdr.game_id != GAME_ID)
    {
        LOGN("Game ID of packet doesn't match, %08X != %08X",pkt->hdr.game_id, GAME_ID);
        return false;
    }

    if(pkt->hdr.type < PACKET_TYPE_INIT || pkt->hdr.type > PACKET_TYPE_STATE)
    {
        LOGN("Invalid Packet Type: %d", pkt->hdr.type);
        return false;
    }

    return true;
}

static bool authenticate_client(Packet* pkt, ClientInfo* cli)
{
    bool valid = true;

    switch(pkt->hdr.type)
    {
        case PACKET_TYPE_CONNECT_REQUEST:
            valid &= (pkt->data_len == MAX_PACKET_DATA_SIZE); // must be padded out to 1024
            valid &= (memcmp(&pkt->data[0],cli->client_salt, 8) == 0);
            break;
        case PACKET_TYPE_CONNECT_CHALLENGE_RESP:
            valid &= (pkt->data_len == MAX_PACKET_DATA_SIZE); // must be padded out to 1024
            valid &= (memcmp(&pkt->data[0],cli->xor_salts, 8) == 0);
            break;
        default:
            valid &= (memcmp(&pkt->data[0],cli->xor_salts, 8) == 0);
            break;
    }

    return valid;
}

static int server_get_client(Address* addr, ClientInfo** cli)
{
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(memcmp(&server.clients[i].address, addr, sizeof(Address)) == 0)
        {
            // found existing client, exit
            *cli = &server.clients[i];
            return i;
        }
    }

    return -1;
}

static bool server_assign_new_client(Address* addr, ClientInfo** cli)
{
    // new client
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(server.clients[i].state == DISCONNECTED)
        {
            *cli = &server.clients[i];
            (*cli)->client_id = i;
            return true;
        }
    }

    LOGN("Server is full and can't accept new clients.");
    return false;
}

static void update_server_num_clients()
{
    int num_clients = 0;
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(server.clients[i].state != DISCONNECTED)
        {
            num_clients++;
        }
    }
    server.num_clients = num_clients;
}

static void remove_client(ClientInfo* cli)
{
    LOGN("Remove client.");
    cli->state = DISCONNECTED;
    cli->remote_latest_packet_id = 0;
    players[cli->client_id].active = false;
    memset(cli,0, sizeof(ClientInfo));
    update_server_num_clients();
}

static void server_send(PacketType type, ClientInfo* cli)
{
    Packet pkt = {
        .hdr.game_id = GAME_ID,
        .hdr.id = server.info.local_latest_packet_id,
        .hdr.ack = cli->remote_latest_packet_id,
        .hdr.type = type
    };

    switch(type)
    {
        case PACKET_TYPE_INIT:
            break;

        case PACKET_TYPE_CONNECT_CHALLENGE:
        {
            uint64_t salt = rand64();
            memcpy(cli->server_salt, (uint8_t*)&salt,8);

            // store xor salts
            store_xor_salts(cli->client_salt, cli->server_salt, cli->xor_salts);
            print_salt(cli->xor_salts);

            pack_bytes(&pkt, cli->client_salt, 8);
            pack_bytes(&pkt, cli->server_salt, 8);

            net_send(&server.info,&cli->address,&pkt);
        } break;

        case PACKET_TYPE_CONNECT_ACCEPTED:
        {
            cli->state = CONNECTED;
            pack_u8(&pkt, (uint8_t)cli->client_id);
            net_send(&server.info,&cli->address,&pkt);
        } break;

        case PACKET_TYPE_CONNECT_REJECTED:
        {
            pack_u8(&pkt, (uint8_t)cli->last_reject_reason);
            net_send(&server.info,&cli->address,&pkt);
        } break;

        case PACKET_TYPE_PING:
            pkt.data_len = 0;
            net_send(&server.info,&cli->address,&pkt);
            break;

        case PACKET_TYPE_STATE:
        {
            pack_u8(&pkt, (uint8_t)game_status);
            pkt.data_len++;

            int num_clients = 0;

            for(int i = 0; i < MAX_CLIENTS; ++i)
            {
                if(server.clients[i].state == CONNECTED)
                {
                    pack_u8(&pkt,(uint8_t)i);
                    pack_vec2(&pkt,server.clients[i].player_state.pos);
                    pack_float(&pkt,server.clients[i].player_state.angle);
                    pack_float(&pkt,server.clients[i].player_state.energy);
                    pack_float(&pkt,server.clients[i].player_state.hp);

                    num_clients++;
                }
            }

            pkt.data[1] = num_clients;

            // if(plist->count > 0)
            {
                pack_u8(&pkt,(uint8_t)plist->count);
            }

            for(int i = 0; i < plist->count; ++i)
            {
                pack_u16(&pkt,projectiles[i].id);
                pack_vec2(&pkt,projectiles[i].pos);
                pack_float(&pkt,projectiles[i].angle_deg);
                pack_u8(&pkt,projectiles[i].player_id);
            }

            //print_packet(&pkt);

            if(memcmp(&cli->prior_state_pkt.data, &pkt.data, pkt.data_len) == 0)
                break;

            net_send(&server.info,&cli->address,&pkt);
            memcpy(&cli->prior_state_pkt, &pkt, get_packet_size(&pkt));

        } break;

        case PACKET_TYPE_ERROR:
        {
            pack_u8(&pkt, (uint8_t)cli->last_packet_error);
            net_send(&server.info,&cli->address,&pkt);
        } break;

        case PACKET_TYPE_SETTINGS:
        {
            int num_clients = 0;

            pkt.data_len = 1;

            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (server.clients[i].state == CONNECTED)
                {
                    pack_u8(&pkt, (uint8_t)i);
                    pack_u8(&pkt, players[i].settings.sprite_index);
                    pack_u32(&pkt, players[i].settings.color);
                    pack_string(&pkt, players[i].settings.name, PLAYER_NAME_MAX);

                    // LOGN("Sending Settings, Client ID: %d", i);
                    // LOGN("  color: 0x%08x", players[i].settings.color);
                    // LOGN("  sprite index: %u", players[i].settings.sprite_index);
                    // LOGN("  name: %s", players[i].settings.name);

                    num_clients++;
                }
            }

            pkt.data[0] = num_clients;

            net_send(&server.info, &cli->address, &pkt);

        } break;

        case PACKET_TYPE_DISCONNECT:
        {
            cli->state = DISCONNECTED;
            pkt.data_len = 0;
            // redundantly send so packet is guaranteed to get through
            for(int i = 0; i < 3; ++i)
                net_send(&server.info,&cli->address,&pkt);
        } break;

        default:
            break;
    }
}

static void server_update_players()
{
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        ClientInfo* cli = &server.clients[i];
        if(cli->state != CONNECTED)
            continue;

        Player* p = &players[cli->client_id];

        //printf("Applying inputs to player. input count: %d\n", cli->input_count);

        projectile_update(1.0/TARGET_FPS);

        if(cli->input_count == 0)
        {
            player_update(p,1.0/TARGET_FPS);
        }
        else
        {
            for(int i = 0; i < cli->input_count; ++i)
            {
                // apply input to player
                for(int j = 0; j < PLAYER_ACTION_MAX; ++j)
                {
                    bool key_state = (cli->net_player_inputs[i].keys & ((uint32_t)1<<j)) != 0;
                    p->actions[j].state = key_state;
                }

                player_update(p,cli->net_player_inputs[i].delta_t);
            }

            cli->input_count = 0;
        }

        projectile_handle_collisions(1.0/TARGET_FPS);

        cli->player_state.pos.x = p->pos.x;
        cli->player_state.pos.y = p->pos.y;
        cli->player_state.angle = p->angle_deg;
        cli->player_state.energy = p->energy;
        cli->player_state.hp = p->hp;
    }
}

static void server_update_game_status()
{
    switch(game_status)
    {
        case GAME_STATUS_LIMBO:
        {
            if(server.num_clients < 2)
                break;

            bool all_players_in_zone = true;

            // check if all players are in ready_zone
            for(int i = 0; i < MAX_CLIENTS; ++i)
            {
                ClientInfo* cli = &server.clients[i];
                if(cli->state != CONNECTED)
                    continue;

                Player* p = &players[cli->client_id];

                if(!rectangles_colliding(&p->hit_box, &ready_zone))
                {
                    all_players_in_zone = false;
                    break;
                }
            }

            if(all_players_in_zone)
            {
                // start game!
                game_status = GAME_STATUS_RUNNING;
            }

        }   break;
        case GAME_STATUS_RUNNING:
        {
            // check to see if there is a winner

        }   break;

        case GAME_STATUS_COMPLETE:
        {

        }   break;
    }

}

int net_server_start()
{
    // init
    socket_initialize();

    memset(server.clients, 0, sizeof(ClientInfo)*MAX_CLIENTS);
    server.num_clients = 0;

    int sock;

    // set timers
    timer_set_fps(&game_timer,TARGET_FPS);
    timer_set_fps(&server_timer,TICK_RATE);

    timer_begin(&game_timer);
    timer_begin(&server_timer);

    LOGN("Creating socket.");
    socket_create(&sock);

    LOGN("Binding socket %u to any local ip on port %u.", sock, PORT);
    socket_bind(sock, NULL, PORT);
    server.info.socket = sock;

    LOGN("Server Started with tick rate %f.", TICK_RATE);

    double t0=timer_get_time();
    double t1=0.0;
    double accum = 0.0;

    double t0_g=timer_get_time();
    double t1_g=0.0;
    double accum_g = 0.0;

    const double dt = 1.0/TICK_RATE;

    for(;;)
    {
        // handle connections, receive inputs
        for(;;)
        {
            // Read all pending packets
            bool data_waiting = has_data_waiting(server.info.socket);
            if(!data_waiting)
            {
                break;
            }

            Address from = {0};
            Packet recv_pkt = {0};
            int offset = 0;

            int bytes_received = net_recv(&server.info, &from, &recv_pkt);

            if(!validate_packet_format(&recv_pkt))
            {
                LOGN("Invalid packet format!");
                timer_delay_us(1000); // delay 1ms
                continue;
            }

            ClientInfo* cli = NULL;

            int client_id = server_get_client(&from, &cli);

            if(client_id == -1) // net client
            {
                if(recv_pkt.hdr.type == PACKET_TYPE_CONNECT_REQUEST)
                {
                    // new client
                    if(recv_pkt.data_len != MAX_PACKET_DATA_SIZE)
                    {
                        LOGN("Packet length doesn't equal %d",MAX_PACKET_DATA_SIZE);
                        remove_client(cli);
                        break;
                    }

                    if(server_assign_new_client(&from, &cli))
                    {
                        cli->state = SENDING_CONNECTION_REQUEST;
                        memcpy(&cli->address,&from,sizeof(Address));
                        update_server_num_clients();

                        LOGN("Welcome New Client! (%d/%d)", server.num_clients, MAX_CLIENTS);
                        print_address(&cli->address);

                        // store salt
                        unpack_bytes(&recv_pkt, cli->client_salt, 8, &offset);
                        server_send(PACKET_TYPE_CONNECT_CHALLENGE, cli);
                    }
                    else
                    {
                        // create a temporary ClientInfo so we can send a reject packet back
                        ClientInfo tmp_cli = {0};
                        memcpy(&tmp_cli.address,&from,sizeof(Address));

                        tmp_cli.last_reject_reason = CONNECT_REJECT_REASON_SERVER_FULL;
                        server_send(PACKET_TYPE_CONNECT_REJECTED, &tmp_cli);
                        break;
                    }
                }
            }
            else
            {
                // existing client
                bool auth = authenticate_client(&recv_pkt,cli);
                offset = 8;

                if(!auth)
                {
                    LOGN("Client Failed authentication");

                    if(recv_pkt.hdr.type == PACKET_TYPE_CONNECT_CHALLENGE_RESP)
                    {
                        cli->last_reject_reason = CONNECT_REJECT_REASON_FAILED_CHALLENGE;
                        server_send(PACKET_TYPE_CONNECT_REJECTED,cli);
                        remove_client(cli);
                    }
                    break;
                }

                bool is_latest = is_packet_id_greater(recv_pkt.hdr.id, cli->remote_latest_packet_id);
                if(!is_latest)
                {
                    LOGN("Not latest packet from client. Ignoring...");
                    timer_delay_us(1000); // delay 1ms
                    break;
                }

                cli->remote_latest_packet_id = recv_pkt.hdr.id;
                cli->time_of_latest_packet = timer_get_time();

                switch(recv_pkt.hdr.type)
                {

                    case PACKET_TYPE_CONNECT_CHALLENGE_RESP:
                    {
                        cli->state = SENDING_CHALLENGE_RESPONSE;
                        players[cli->client_id].active = true;

                        server_send(PACKET_TYPE_CONNECT_ACCEPTED,cli);
                        server_send(PACKET_TYPE_STATE,cli);
                    } break;

                    case PACKET_TYPE_INPUT:
                    {
                        uint8_t _input_count = unpack_u8(&recv_pkt, &offset);
                        for(int i = 0; i < _input_count; ++i)
                        {
                            // get input, copy into array
                            unpack_bytes(&recv_pkt, (uint8_t*)&cli->net_player_inputs[cli->input_count++], sizeof(NetPlayerInput), &offset);
                        }
                    } break;

                    case PACKET_TYPE_SETTINGS:
                    {
                        Player* p = &players[cli->client_id];

                        uint8_t sprite_index = unpack_u8(&recv_pkt, &offset);
                        p->settings.sprite_index = sprite_index;

                        uint32_t color = unpack_u32(&recv_pkt, &offset);
                        p->settings.color = color;

                        memset(p->settings.name, 0, PLAYER_NAME_MAX);
                        uint8_t namelen = unpack_string(&recv_pkt, p->settings.name, PLAYER_NAME_MAX, &offset);

                        // LOGN("Server Received Settings, Client ID: %d", cli->client_id);
                        // LOGN("  color: 0x%08x", p->settings.color);
                        // LOGN("  sprite index: %u", p->settings.sprite_index);
                        // LOGN("  name (%u): %s", namelen, p->settings.name);

                        for(int i = 0; i < MAX_CLIENTS; ++i)
                        {
                            ClientInfo* cli = &server.clients[i];
                            if(cli == NULL) continue;
                            if(cli->state != CONNECTED) continue;

                            server_send(PACKET_TYPE_SETTINGS,cli);
                        }
                    } break;

                    case PACKET_TYPE_PING:
                    {
                        server_send(PACKET_TYPE_PING, cli);
                    } break;

                    case PACKET_TYPE_DISCONNECT:
                    {
                        remove_client(cli);
                    } break;

                    default:
                    break;
                }
            }

            //timer_delay_us(1000); // delay 1ms
        }


        t1_g = timer_get_time();
        double elapsed_time_g = t1_g - t0_g;
        t0_g = t1_g;

        accum_g += elapsed_time_g;
        while(accum_g >= 1.0/TARGET_FPS)
        {
            server_update_players();
            accum_g -= 1.0/TARGET_FPS;
        }

        server_update_game_status();

        t1 = timer_get_time();
        double elapsed_time = t1 - t0;
        t0 = t1;

        accum += elapsed_time;

        if(accum >= dt)
        {
            // send state packet to all clients
            if(server.num_clients > 0)
            {
                // disconnect any client that hasn't sent a packet in DISCONNECTION_TIMEOUT
                for(int i = 0; i < MAX_CLIENTS; ++i)
                {
                    ClientInfo* cli = &server.clients[i];

                    if(cli == NULL) continue;
                    if(cli->state == DISCONNECTED) continue;

                    if(cli->time_of_latest_packet > 0)
                    {
                        double time_elapsed = timer_get_time() - cli->time_of_latest_packet;

                        if(time_elapsed >= DISCONNECTION_TIMEOUT)
                        {
                            LOGN("Client timed out. Elapsed time: %f", time_elapsed);

                            // disconnect client
                            server_send(PACKET_TYPE_DISCONNECT,cli);
                            remove_client(cli);
                            continue;
                        }
                    }

                    // send world state to connected clients...
                    server_send(PACKET_TYPE_STATE,cli);
                }
            }
            accum = 0.0;
        }

        // don't wait, just proceed to handling packets
        //timer_wait_for_frame(&server_timer);
        timer_delay_us(1000);
    }
}


// =========
// @CLIENT
// =========

struct
{
    Address address;
    NodeInfo info;
    ConnectionState state;
    CircBuf input_packets;
    double time_of_latest_sent_packet;
    double time_of_last_ping;
    double time_of_last_received_ping;
    double rtt;
    uint8_t player_count;
    uint8_t server_salt[8];
    uint8_t client_salt[8];
    uint8_t xor_salts[8];
} client = {0};

bool net_client_add_player_input(NetPlayerInput* input)
{
    if(input_count >= INPUT_QUEUE_MAX)
    {
        LOGW("Input array is full!");
        return false;
    }

    memcpy(&net_player_inputs[input_count], input, sizeof(NetPlayerInput));
    input_count++;

    return true;
}

int net_client_get_input_count()
{
    return input_count;
}

uint8_t net_client_get_player_count()
{
    return client.player_count;
}

ConnectionState net_client_get_state()
{
    return client.state;
}


uint16_t net_client_get_latest_local_packet_id()
{
    return client.info.local_latest_packet_id;
}

void net_client_get_server_ip_str(char* ip_str)
{
    if(!ip_str)
        return;

    sprintf(ip_str,"%u.%u.%u.%u:%u",server.address.a,server.address.b, server.address.c, server.address.d, server.address.port);
    return;
}

bool net_client_set_server_ip(char* address)
{
    // example input:
    // 200.100.24.10

    char num_str[3] = {0};
    uint8_t   bytes[4]  = {0};

    uint8_t   num_str_index = 0, byte_index = 0;

    for(int i = 0; i < strlen(address)+1; ++i)
    {
        if(address[i] == '.' || address[i] == '\0')
        {
            bytes[byte_index++] = atoi(num_str);
            memset(num_str,0,3*sizeof(char));
            num_str_index = 0;
            continue;
        }

        num_str[num_str_index++] = address[i];
    }

    server.address.a = bytes[0];
    server.address.b = bytes[1];
    server.address.c = bytes[2];
    server.address.d = bytes[3];

    server.address.port = PORT;

    return true;
}

// client information
bool net_client_init()
{
    socket_initialize();

    int sock;

    LOGN("Creating socket.");
    socket_create(&sock);

    client.info.socket = sock;
    circbuf_create(&client.input_packets,10, sizeof(Packet));

    return true;
}

bool net_client_data_waiting()
{
    bool data_waiting = has_data_waiting(client.info.socket);
    return data_waiting;
}

static void client_send(PacketType type)
{
    Packet pkt = {
        .hdr.game_id = GAME_ID,
        .hdr.id = client.info.local_latest_packet_id,
        .hdr.ack = client.info.remote_latest_packet_id,
        .hdr.type = type
    };

    memset(pkt.data, 0, MAX_PACKET_DATA_SIZE);

    switch(type)
    {
        case PACKET_TYPE_CONNECT_REQUEST:
        {
            uint64_t salt = rand64();
            memcpy(client.client_salt, (uint8_t*)&salt,8);

            pack_bytes(&pkt, (uint8_t*)client.client_salt, 8);
            pkt.data_len = MAX_PACKET_DATA_SIZE; // pad to 1024

            net_send(&client.info,&server.address,&pkt);
        } break;

        case PACKET_TYPE_CONNECT_CHALLENGE_RESP:
        {
            store_xor_salts(client.client_salt, client.server_salt, client.xor_salts);

            pack_bytes(&pkt, (uint8_t*)client.xor_salts, 8);
            pkt.data_len = MAX_PACKET_DATA_SIZE; // pad to 1024

            net_send(&client.info,&server.address,&pkt);
        } break;

        case PACKET_TYPE_PING:
        {
            pack_bytes(&pkt, (uint8_t*)client.xor_salts, 8);
            net_send(&client.info,&server.address,&pkt);
        } break;

        case PACKET_TYPE_INPUT:
        {
            pack_bytes(&pkt, (uint8_t*)client.xor_salts, 8);
            pack_u8(&pkt, input_count);
            for(int i = 0; i < input_count; ++i)
            {
                pack_bytes(&pkt, (uint8_t*)&net_player_inputs[i], sizeof(NetPlayerInput));
            }

            circbuf_add(&client.input_packets,&pkt);
            net_send(&client.info,&server.address,&pkt);
        } break;

        case PACKET_TYPE_SETTINGS:
        {
            pack_bytes(&pkt, (uint8_t*)client.xor_salts, 8);
            pack_u8(&pkt, player->settings.sprite_index);
            pack_u32(&pkt, player->settings.color);
            pack_string(&pkt, player->settings.name, PLAYER_NAME_MAX);

            // LOGN("Client Send Settings");
            // LOGN("  color: 0x%08x", player->settings.color);
            // LOGN("  sprite index: %u", player->settings.sprite_index);
            // LOGN("  name (%d): %s", strlen(player->settings.name), player->settings.name);

            net_send(&client.info,&server.address,&pkt);
        } break;

        case PACKET_TYPE_DISCONNECT:
        {
            pack_bytes(&pkt, (uint8_t*)client.xor_salts, 8);

            // redundantly send so packet is guaranteed to get through
            for(int i = 0; i < 3; ++i)
            {
                net_send(&client.info,&server.address,&pkt);
                pkt.hdr.id = client.info.local_latest_packet_id;
            }
        } break;

        default:
            break;
    }

    client.time_of_latest_sent_packet = timer_get_time();
}

static bool client_get_input_packet(Packet* input, int packet_id)
{
    for(int i = client.input_packets.count -1; i >= 0; --i)
    {
        Packet* pkt = (Packet*)circbuf_get_item(&client.input_packets,i);

        if(pkt)
        {
            if(pkt->hdr.id == packet_id)
            {
                input = pkt;
                return true;
            }
        }
    }
    return false;
}

int net_client_connect()
{
    if(client.state != DISCONNECTED)
        return -1; // temporary, handle different states in the future

    for(;;)
    {
        client.state = SENDING_CONNECTION_REQUEST;
        client_send(PACKET_TYPE_CONNECT_REQUEST);

        for(;;)
        {
            bool data_waiting = net_client_data_waiting();

            if(!data_waiting)
            {
                double time_elapsed = timer_get_time() - client.time_of_latest_sent_packet;
                if(time_elapsed >= DEFAULT_TIMEOUT)
                    break; // retry sending

                timer_delay_us(1000); // delay 1ms
                continue;
            }

            Packet srvpkt = {0};
            int offset = 0;

            int recv_bytes = net_client_recv(&srvpkt);
            if(recv_bytes > 0)
            {
                switch(srvpkt.hdr.type)
                {
                    case PACKET_TYPE_CONNECT_CHALLENGE:
                    {
                        uint8_t srv_client_salt[8] = {0};
                        unpack_bytes(&srvpkt, srv_client_salt, 8, &offset);
                        if(memcmp(srv_client_salt, client.client_salt, 8) != 0)
                        {
                            LOGN("Server sent client salt doesn't match actual client salt");
                            return -1;
                        }

                        unpack_bytes(&srvpkt, client.server_salt, 8, &offset);
                        LOGN("Received Connect Challenge.");

                        client.state = SENDING_CHALLENGE_RESPONSE;
                        client_send(PACKET_TYPE_CONNECT_CHALLENGE_RESP);
                    } break;

                    case PACKET_TYPE_CONNECT_ACCEPTED:
                    {
                        client.state = CONNECTED;
                        uint8_t client_id = unpack_u8(&srvpkt, &offset);
                        return (int)client_id;
                    } break;

                    case PACKET_TYPE_CONNECT_REJECTED:
                    {
                        uint8_t reason = unpack_u8(&srvpkt, &offset);
                        LOGN("Rejection Reason: %s (%02X)", connect_reject_reason_to_str(reason), reason);
                        client.state = DISCONNECTED; // TODO: is this okay?
                    } break;
                }
            }

            timer_delay_us(1000); // delay 1000 us
        }
    }
}

void net_client_connect_request()
{
    client.state = SENDING_CONNECTION_REQUEST;
    client_send(PACKET_TYPE_CONNECT_REQUEST);
}

// 0: no data waiting
// 1: timeout
// 2: got data
int net_client_connect_data_waiting()
{
    bool data_waiting = net_client_data_waiting();

    if(!data_waiting)
    {
        double time_elapsed = timer_get_time() - client.time_of_latest_sent_packet;
        if(time_elapsed >= DEFAULT_TIMEOUT)
        {
            client.state = DISCONNECTED; //TODO
            return 1;
        }

        return 0;
    }

    return 2;
}

int net_client_connect_recv_data()
{
    Packet srvpkt = {0};
    int offset = 0;

    int recv_bytes = net_client_recv(&srvpkt);
    if(recv_bytes > 0)
    {
        switch(srvpkt.hdr.type)
        {
            case PACKET_TYPE_CONNECT_CHALLENGE:
            {
                uint8_t srv_client_salt[8] = {0};
                unpack_bytes(&srvpkt, srv_client_salt, 8, &offset);

                if(memcmp(srv_client_salt, client.client_salt, 8) != 0)
                {
                    LOGN("Server sent client salt doesn't match actual client salt");
                    return CONN_RC_INVALID_SALT;
                }

                unpack_bytes(&srvpkt, client.server_salt, 8, &offset);
                LOGN("Received Connect Challenge.");

                client.state = SENDING_CHALLENGE_RESPONSE;
                client_send(PACKET_TYPE_CONNECT_CHALLENGE_RESP);
                return CONN_RC_CHALLENGED;
            } break;

            case PACKET_TYPE_CONNECT_ACCEPTED:
            {
                client.state = CONNECTED;
                uint8_t client_id = unpack_u8(&srvpkt, &offset);
                return (int)client_id;
            } break;

            case PACKET_TYPE_CONNECT_REJECTED:
            {
                uint8_t reason = unpack_u8(&srvpkt, &offset);
                LOGN("Rejection Reason: %s (%02X)", connect_reject_reason_to_str(reason), reason);
                client.state = DISCONNECTED; // TODO: is this okay?
                return CONN_RC_REJECTED;
            } break;
        }
    }
    return CONN_RC_NO_DATA;
}

void net_client_update()
{
    bool data_waiting = net_client_data_waiting(); // non-blocking

    if(data_waiting)
    {
        Packet srvpkt = {0};
        int offset = 0;

        int recv_bytes = net_client_recv(&srvpkt);

        bool is_latest = is_packet_id_greater(srvpkt.hdr.id, client.info.remote_latest_packet_id);

        if(recv_bytes > 0 && is_latest)
        {
            switch(srvpkt.hdr.type)
            {
                case PACKET_TYPE_STATE:
                {

                    uint8_t gs = unpack_u8(&srvpkt, &offset);

                    uint8_t num_players = unpack_u8(&srvpkt, &offset);
                    client.player_count = num_players;

                    for(int i = 0; i < MAX_CLIENTS; ++i)
                        players[i].active = false;

                    //LOGN("Received STATE packet. num players: %d", num_players);

                    for(int i = 0; i < num_players; ++i)
                    {

                        uint8_t client_id = unpack_u8(&srvpkt, &offset);

                        //LOGN("  %d: Client ID %d", i, client_id);

                        if(client_id >= MAX_CLIENTS)
                        {
                            LOGE("Client ID is too large: %d",client_id);
                            break;
                        }

                        Vector2f pos = unpack_vec2(&srvpkt, &offset);
                        float angle  = unpack_float(&srvpkt, &offset);
                        float energy = unpack_float(&srvpkt, &offset);
                        float hp     = unpack_float(&srvpkt, &offset);

                        //LOGN("      Pos: %f, %f. Angle: %f", pos.x, pos.y, angle);

                        Player* p = &players[client_id];
                        p->active = true;

                        p->lerp_t = 0.0;

                        p->server_state_prior.pos.x = p->pos.x;
                        p->server_state_prior.pos.y = p->pos.y;
                        p->server_state_prior.angle = p->angle_deg;
                        p->server_state_prior.energy = p->energy;
                        p->server_state_prior.hp = p->hp;

                        p->server_state_target.pos.x = pos.x;
                        p->server_state_target.pos.y = pos.y;
                        p->server_state_target.angle = angle;
                        p->server_state_target.energy = energy;
                        p->server_state_target.hp = hp;
                    }

                    // if(offset < srvpkt.data_len-1)
                    {
                        // load projectiles
                        uint8_t num_projectiles = unpack_u8(&srvpkt, &offset);

                        list_clear(plist);
                        plist->count = num_projectiles;

                        for(int i = 0; i < num_projectiles; ++i)
                        {
                            Projectile* p = &projectiles[i];

                            uint16_t id = unpack_u16(&srvpkt, &offset);
                            Vector2f pos = unpack_vec2(&srvpkt, &offset);
                            float angle = unpack_float(&srvpkt, &offset);
                            uint8_t player_id = unpack_u8(&srvpkt, &offset);

                            p->lerp_t = 0.0;

                            //LOGN("      Pos: %f, %f. Angle: %f", pos.x, pos.y, angle);

                            p->server_state_prior.id = p->id;
                            p->server_state_prior.pos.x = p->pos.x;
                            p->server_state_prior.pos.y = p->pos.y;
                            p->server_state_prior.angle = p->angle_deg;

                            p->server_state_target.id = id;
                            p->server_state_target.pos.x = pos.x;
                            p->server_state_target.pos.y = pos.y;
                            p->server_state_target.angle = angle;
                        }
                    }

                    client.player_count = num_players;
                    player_count = num_players;

                    if(gs >= 0 && gs < GAME_STATUS_MAX)
                    {
                        // GameStatus gs_prior = game_status;
                        if(gs != game_status)
                        {
                            game_status = gs;
                            switch(game_status)
                            {
                                case GAME_STATUS_LIMBO:
                                    screen = SCREEN_GAME_START;
                                    break;
                                case GAME_STATUS_RUNNING:
                                    screen = SCREEN_GAME;
                                    break;
                                case GAME_STATUS_COMPLETE:
                                    screen = SCREEN_GAME_END;
                                    break;
                                default:
                                    break;
                            }
                        }

                    }

                } break;
 
                case PACKET_TYPE_SETTINGS:
                {
                    uint8_t num_players = unpack_u8(&srvpkt, &offset);

                    for(int i = 0; i < num_players; ++i)
                    {
                        uint8_t client_id = unpack_u8(&srvpkt, &offset);

                        //LOGN("  %d: Client ID %d", i, client_id);

                        if(client_id >= MAX_CLIENTS)
                        {
                            LOGE("Client ID is too large: %d", client_id);
                            break;
                        }

                        Player* p = &players[client_id];
                        p->settings.sprite_index = unpack_u8(&srvpkt, &offset);
                        p->settings.color = unpack_u32(&srvpkt, &offset);
                        uint8_t namelen = unpack_string(&srvpkt, p->settings.name, PLAYER_NAME_MAX, &offset);

                        LOGN("Client Received Settings, Client ID: %d", client_id);
                        LOGN("  color: 0x%08x", p->settings.color);
                        LOGN("  sprite index: %u", p->settings.sprite_index);
                        LOGN("  name (%u): %s", namelen, p->settings.name);

                    }
                } break;

                case PACKET_TYPE_PING:
                {
                    client.time_of_last_received_ping = timer_get_time();
                    client.rtt = 1000.0f*(client.time_of_last_received_ping - client.time_of_last_ping);
                } break;

                case PACKET_TYPE_DISCONNECT:
                    client.state = DISCONNECTED;
                    break;
            }
        }
    }

    // handle pinging server
    double time_elapsed = timer_get_time() - client.time_of_last_ping;
    if(time_elapsed >= PING_PERIOD)
    {
        client_send(PACKET_TYPE_PING);
        client.time_of_last_ping = timer_get_time();
    }

    // handle publishing inputs
    if(input_count >= inputs_per_packet)
    {
        client_send(PACKET_TYPE_INPUT);
        input_count = 0;
    }
}

bool net_client_is_connected()
{
    return (client.state == CONNECTED);
}

double net_client_get_rtt()
{
    return client.rtt;
}

void net_client_disconnect()
{
    if(client.state != DISCONNECTED)
    {
        client_send(PACKET_TYPE_DISCONNECT);
        client.state = DISCONNECTED;
    }
}

void net_client_send_settings()
{
    client_send(PACKET_TYPE_SETTINGS);
}

int net_client_send(uint8_t* data, uint32_t len)
{
    Packet pkt = {
        .hdr.game_id = GAME_ID,
        .hdr.id = client.info.local_latest_packet_id,
    };

    memcpy(pkt.data,data,len);
    pkt.data_len = len;

    int sent_bytes = net_send(&client.info, &server.address, &pkt);
    return sent_bytes;
}

int net_client_recv(Packet* pkt)
{
    Address from = {0};
    int recv_bytes = net_recv(&client.info, &from, pkt);
    return recv_bytes;
}

void net_client_deinit()
{
    socket_close(client.info.socket);
}


static inline void pack_u8(Packet* pkt, uint8_t d)
{
    pkt->data[pkt->data_len] = d;
    pkt->data_len += sizeof(uint8_t);
}

static inline void pack_u16(Packet* pkt, uint16_t d)
{
    pkt->data[pkt->data_len+0] = (d>>8) & 0xFF;
    pkt->data[pkt->data_len+1] = (d) & 0xFF;

    pkt->data_len+=sizeof(uint16_t);
}

static inline void pack_u32(Packet* pkt, uint32_t d)
{
    pkt->data[pkt->data_len+0] = (d>>24) & 0xFF;
    pkt->data[pkt->data_len+1] = (d>>16) & 0xFF;
    pkt->data[pkt->data_len+2] = (d>>8) & 0xFF;
    pkt->data[pkt->data_len+3] = (d) & 0xFF;

    pkt->data_len+=sizeof(uint32_t);
}

static inline void pack_u64(Packet* pkt, uint64_t d)
{
    pkt->data[pkt->data_len+0] = (d>>56) & 0xFF;
    pkt->data[pkt->data_len+1] = (d>>48) & 0xFF;
    pkt->data[pkt->data_len+2] = (d>>40) & 0xFF;
    pkt->data[pkt->data_len+3] = (d>>32) & 0xFF;
    pkt->data[pkt->data_len+4] = (d>>24) & 0xFF;
    pkt->data[pkt->data_len+5] = (d>>16) & 0xFF;
    pkt->data[pkt->data_len+6] = (d>>8) & 0xFF;
    pkt->data[pkt->data_len+7] = (d) & 0xFF;

    pkt->data_len+=sizeof(uint64_t);
}


static inline void pack_float(Packet* pkt, float d)
{
    memcpy(&pkt->data[pkt->data_len],&d,sizeof(float));
    pkt->data_len+=sizeof(float);
}

static inline void pack_bytes(Packet* pkt, uint8_t* d, uint8_t len)
{
    memcpy(&pkt->data[pkt->data_len],d,sizeof(uint8_t)*len);
    pkt->data_len+=len*sizeof(uint8_t);
}

static inline void pack_string(Packet* pkt, char* s, uint8_t max_len)
{
    uint8_t slen = (uint8_t)MIN(max_len,strlen(s));
    pkt->data[pkt->data_len] = slen;
    pkt->data_len++;

    memcpy(&pkt->data[pkt->data_len],s,slen*sizeof(char));
    pkt->data_len += slen*sizeof(char);
}

static inline void pack_vec2(Packet* pkt, Vector2f d)
{
    memcpy(&pkt->data[pkt->data_len],&d,sizeof(Vector2f));
    pkt->data_len+=sizeof(Vector2f);
}


static inline uint8_t  unpack_u8(Packet* pkt, int* offset)
{
    uint8_t r = pkt->data[*offset];
    (*offset)++;
    return r;
}

static inline uint16_t unpack_u16(Packet* pkt, int* offset)
{
    uint16_t r = pkt->data[*offset] << 8 | pkt->data[*offset+1];
    (*offset)+=sizeof(uint16_t);
    return r;
}

static inline uint32_t unpack_u32(Packet* pkt, int* offset)
{
    uint32_t r = pkt->data[*offset] << 24 | pkt->data[*offset+1] << 16 | pkt->data[*offset+2] << 8 | pkt->data[*offset+3];
    (*offset)+=sizeof(uint32_t);
    return r;
}

static inline uint64_t unpack_u64(Packet* pkt, int* offset)
{
    uint64_t r = (uint64_t)(pkt->data[*offset]) << 56;
    r |= (uint64_t)(pkt->data[*offset+1]) << 48;
    r |= (uint64_t)(pkt->data[*offset+2]) << 40;
    r |= (uint64_t)(pkt->data[*offset+3]) << 32;
    r |= (uint64_t)(pkt->data[*offset+4]) << 24;
    r |= (uint64_t)(pkt->data[*offset+5]) << 16;
    r |= (uint64_t)(pkt->data[*offset+6]) << 8;
    r |= (uint64_t)(pkt->data[*offset+7]);

    (*offset)+=sizeof(uint64_t);
    return r;
}

static inline float unpack_float(Packet* pkt, int* offset)
{
    float r;
    memcpy(&r, &pkt->data[*offset], sizeof(float));
    (*offset) += sizeof(float);
    return r;
}

static inline void unpack_bytes(Packet* pkt, uint8_t* d, int len, int* offset)
{
    memcpy(d, &pkt->data[*offset], len*sizeof(uint8_t));
    (*offset) += len*sizeof(uint8_t);
}


static inline uint8_t unpack_string(Packet* pkt, char* s, int maxlen, int* offset)
{
    uint8_t len = unpack_u8(pkt, offset);
    if(len > maxlen)
        LOGW("unpack_string(): len > maxlen (%u > %u)", len, maxlen);

    uint8_t copy_len = MIN(len,maxlen);
    memset(s, 0, maxlen);
    memcpy(s, &pkt->data[*offset], copy_len*sizeof(char));
    (*offset) += len; //traverse the actual total length
    return copy_len;
}

static inline Vector2f unpack_vec2(Packet* pkt, int* offset)
{
    Vector2f r;
    memcpy(&r, &pkt->data[*offset], sizeof(Vector2f));
    (*offset) += sizeof(Vector2f);
    return r;
}

void test_packing()
{


    Packet pkt = {0};

    uint16_t id0;
    Vector2f pos0;
    float a0;
    uint8_t pid0;
    uint32_t color0;
    uint64_t u64_0;

    id0 = 12037;
    pack_u16(&pkt,id0);

    pos0.x = 123.456;
    pos0.y = 69.420;
    pack_vec2(&pkt,pos0);

    a0 = 234.1354;
    pack_float(&pkt,a0);

    pid0 = 85;
    pack_u8(&pkt,pid0);

    color0 = 2342612;
    pack_u32(&pkt,color0);

    u64_0 = 0xab000000cd000001;
    pack_u64(&pkt,u64_0);


    LOGI("Packet Length: %d", pkt.data_len);
    print_hex(pkt.data, pkt.data_len);


    uint16_t id1;
    Vector2f pos1;
    float a1;
    uint8_t pid1;
    uint32_t color1;
    uint64_t u64_1;

    int index = 0;
    id1 = unpack_u16(&pkt, &index);
    pos1 = unpack_vec2(&pkt, &index);
    a1 = unpack_float(&pkt, &index);
    pid1 = unpack_u8(&pkt, &index);
    color1 = unpack_u32(&pkt, &index);
    u64_1 = unpack_u64(&pkt, &index);

    LOGI("Unpack final index: %d", index);

    LOGI("ID: %u  =  %u", id0, id1);
    LOGI("Pos: %.3f, %.3f  =  %.3f, %.3f", pos0.x, pos0.y, pos1.x, pos1.y);
    LOGI("Angle: %.4f  =  %.4f", a0, a1);
    LOGI("P ID: %u  =  %u", pid0, pid1);
    LOGI("Color: %u  =  %u", color0, color1);
    LOGI("u64: %llu  =  %llu", u64_0, u64_1);
}
