#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>  
#include <sys/socket.h>  

#include "packet.h"

#include "../args.h"  
#include "../nettypes.h"  

typedef enum  {
    UNAUTHENTICATED,
    IDLE,
    WAITING,
} eConnectionState;

// ------------------------------------------------------------ 
typedef uint64_t node_id_t;

typedef struct {
    bool open;
    uint64_t tx;
    uint64_t rx;
} xConnectionStatus;


// ------------------------------------------------------------ 
// PEER SHIT
typedef struct {
    node_id_t node_id;
    Address ip;   

    int stream_fd;           

    xConnectionStatus   status;
    eConnectionState     state;
} xPeerConnection;

// ------------------------------------------------------------ 
typedef struct {
    Address ip;
    node_id_t node_id;
} xWhoAmI;
// ------------------------------------------------------------ 

typedef union {
    
  struct StateConnecting { node_id_t waiting_peer_id; } StateConnecting;

  struct StateIndexWaitingPeers { uint64_t connected; } StateIndexWaitingPeers;



} uMachineState;

// ------------------------------------------------------------ 
typedef enum  {

    SERVER_BOOTING              = 0,
    SERVER_CONNECTING           = 1,

    SERVER_IDLE                 = 2,
  
    //- 
    SERVER_BEGIN_OPERATION      ,

    // iDX SPECIFIC
    SERVER_INDEX_PRESENT_ITSELF,
    SERVER_INDEX_WAITING_PEERS,


    // non-iDX specifics
    SERVER_WAIT_INDEX_GOSSIP,
    SERVER_REPORT_KNOWLEDGE_TO_INDEX,



    SERVER_OTHER      = 99
} eServerState;

// ------------------------------------------------------------ 


typedef struct {

  Address *peer_ips; // malloc'ed list
  size_t known_peers;


} xIndexData;

// ------------------------------------------------------------ 
//  THIS IS STACK, so 
typedef struct {
    xWhoAmI me;
    xPeerConnection peer_f;     // Forward peer
    xPeerConnection peer_b;     // Backward peer
                                //
    eServerState state;
    uMachineState machine_state;  


    uint64_t net_size;

    // 
    xPeerConnection index;
    xIndexData* index_data;
    

    int listener_fd;            // TCP listener socket
    int *peers;                 // Malloc'ed clients based on netsize
                                //
} Server;


// ------------------------------------------------------------
typedef struct {
  node_id_t sender_id;
  node_id_t index_id;
  Address index_addr;
} xIndexPresentationPacket;

typedef enum  {
  THE_LEADER_IS_DEAD          = 0,
  IM_CLIENT_YOU_MUST_OBEY_ME  = 10,
} eCommandType;

typedef struct {
  node_id_t sender_id;
  eCommandType type;

  union {

    struct {
      uint64_t part_id;
      uint64_t part_size;
    } file_segment;

  } content;

} xCommandPacket;
  
typedef struct {

  union PacketType {

    xIndexPresentationPacket index_presentation_pkt; 

    xCommandPacket cmd; 

    uint8_t raw[4096];

  } bytes;

  uint16_t size; 
} xPacket;

// ------------------------------------------------------------ 

int server_init(Server *sv, const Args *opts);
int server_open(Server *sv);
void server_close(Server *sv);
void server_set_state(Server *sv, eServerState st);

bool server_is_peerf_connected(const Server *sv);
bool server_is_peerb_connected(const Server *sv);



int server_dial_peer(Server *sv);
int server_accept(Server *sv);
void server_close_socket(Server *sv, int socket);


int server_dial_index(Server *sv);

size_t server_send_to_peer_f(Server *sv, xPacket *packet);
xPacket server_wait_from_peer_b(Server *sv);





#endif // SERVER_H

