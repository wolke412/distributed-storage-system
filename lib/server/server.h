#ifndef SERVER_H
#define SERVER_H




#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <memory.h>  

#include <poll.h>  


#include "../args.h"  
#include "../nettypes.h"  

#include "../fileserver/fs.h"  


#define CLIENT_NODE_ID              ((node_id_t)((2<<64) - 1))


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





/**
 * 
 *  ------------------------------------------------------------ 
 */
typedef struct xIndexPresentationPacket {
  node_id_t sender_id;
  node_id_t index_id;
  Address index_addr;
} xIndexPresentationPacket;

typedef struct xPeerIsDeadMessage{
  node_id_t dead_peer_id;
  Address sender_address;
} xPeerIsDeadMessage;

typedef struct xPeerReportMessage{
  node_id_t peer_id;
  Address peer_addr;
} xPeerReportMessage;

typedef struct xRequestFileCreation{
  char name[256];
  uint64_t file_size;
} xRequestFileCreation;

typedef struct xResponseFileCreation{
  uint64_t buffer_limit;
} xResponseFileCreation;

//-

typedef enum { // force to uint8_t

  TYPE_LEADER_IS_DEAD           = 0 , // disseminates panic in the network

  TYPE_PRESENT_ITSELF           = 1,

  TYPE_PEER_IS_DEAD              , // do you have what it takes to crush my rat? 
                                   //

  TYPE_REPORT_PEER, 

  TYPE_REQUEST_FILE_INDEX, 


  TYPE_CREATE_FILE = 10,

} eMessageType;


typedef struct __attribute((packed)) {

  node_id_t sender_id;

  uint8_t type;

  union {
    // ----------------------------------------
    xPeerIsDeadMessage dead_peer;
    // ----------------------------------------
    xPeerReportMessage report_peer;
    // ----------------------------------------
    xRequestFileCreation create_file;
    xResponseFileCreation res_create_file;
    // ----------------------------------------
    // xPeerReportMessage report_peer;
    // ----------------------------------------

  } content;

} xCommunicationPacket;
  
typedef struct {

  union PacketType {
    
      // this is dumb. but il keep it to remind me of how dumb one can be
    xIndexPresentationPacket index_presentation_pkt; 

    // this should be enough
    xCommunicationPacket comm; 

    uint8_t raw[4096];

  } bytes;

  int16_t size; 

} xPacket;

// ------------------------------------------------------------ 






typedef union {
    
  struct StateConnecting { node_id_t waiting_peer_id; } StateConnecting;

  struct StateIndexWaitingPeers { uint64_t connected; } StateIndexWaitingPeers;

  struct StateRawPackets { bool uploading; size_t n_pkts; uint64_t total_size; int client_fd; char *buffer; xRequestFileCreation fc; } StateRawPackets;

  struct StateReceivedPacket { xPacket packet; int from_fd; } StateReceivedPacket ;

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
    SERVER_INDEX_HANDLE_NEW_FILE,


    // non-iDX specifics
    SERVER_WAIT_INDEX_GOSSIP,
    SERVER_REPORT_KNOWLEDGE_TO_INDEX,



    SERVER_RECEIVED_PACKET,

    SERVER_WAITING_RAW_PACKETS,


    SERVER_OTHER      = 99
} eServerState;

// ------------------------------------------------------------ 




typedef struct xIndexData {
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
    int client_fd;            // TCP listener socket

} Server;



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


void server_index_save_reported_peer(Server *sv, xPacket *packet);


size_t server_send_to_peer_f(Server *sv, xPacket *packet);
size_t server_send_to_index(Server *sv, xPacket *packet);
size_t server_send_to_socket(Server *sv, xPacket *packet, int fd);

xPacket server_wait_from_peer_b(Server *sv);
xPacket server_wait_from_socket(Server *sv, int fd);

node_id_t server_wait_client_presentation(Server *sv, int c);


void server_onxpacket(Server *sv);
void server_handle_raw_packets(Server *sv);


// ------------------------------------------------------------
// XPACKET SHIT
// ------------------------------------------------------------

xPacket xpacket_report_peer( Server *sv );
xPacket xpacket_presentation( Server *sv );
void xpacket_debug(const xPacket *p);

#endif // SERVER_H

