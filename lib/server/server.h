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
#include "../defines.h"  

#include "../fileserver/fs.h"  


#define CLIENT_NODE_ID              ((node_id_t)((2<<64) - 1))


// returns current milliseconds
uint64_t current_millis();


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
  Address peer_addr;
} xPeerReportMessage;

typedef struct xRequestFileCreation{
  char name[256];
  uint64_t file_size;
} xRequestFileCreation;

typedef struct xResponseFileCreation{
  uint64_t buffer_limit;
} xResponseFileCreation;

typedef struct xRequestFragmentCreation{
  char file_name[256];
  uint64_t file_size;
  uint64_t file_id;
  uint8_t fragment_count_total;
    
  uint64_t frag_id;
  uint64_t frag_size;
} xRequestFragmentCreation;

// -

typedef struct xRequestFile{
  char name[256];
} xRequestFile;

typedef struct xResponseRequestFile{
  uint64_t  file_size;
  uint64_t  file_id;
  uint8_t   fragment_count_total;
} xResponseRequestFile;

typedef struct xDeliverFragmentTo{
  uint64_t  file_id;
  uint64_t  frag_id;
  Address   to;
} xDeliverFragmentTo;

typedef struct xDeclareFragmentTransport{
  uint64_t file_id;
  uint64_t frag_id;
  uint64_t frag_size;
  uint64_t file_size;
} xDeclareFragmentTransport;

// -



//-

typedef enum { // force to uint8_t

  TYPE_LEADER_IS_DEAD           = 0 , // disseminates panic in the network

  TYPE_PRESENT_ITSELF           = 1,

  TYPE_PEER_IS_DEAD              , // do you have what it takes to crush my rat? 
                                   //

  TYPE_REPORT_SELF, 

  TYPE_REQUEST_FILE_INDEX, 


  // -
  TYPE_CREATE_FILE        = 10,
  TYPE_STORE_FRAGMENT     = 11,

  TYPE_REQUEST_FILE       = 15,
  TYPE_RESPONSE_FILE      = 16,

  TYPE_REQUEST_FRAG       = 20,
  TYPE_DECLARE_FRAG       = 21,

  TYPE_OK     = 200, 
  TYPE_NOT_OK = 220, 

} eMessageType;


typedef struct __attribute((packed)) {

  node_id_t sender_id;

  uint8_t type;

  union {
    // ----------------------------------------
    xPeerIsDeadMessage dead_peer;
    // ----------------------------------------
    xPeerReportMessage report_self;
    // ----------------------------------------
    xRequestFileCreation create_file;
    xResponseFileCreation res_create_file;
    // ----------------------------------------
    xRequestFile              request_file;
    xResponseRequestFile      request_file_response;
    xDeliverFragmentTo        deliver_fragment_to;
    xDeclareFragmentTransport declare_fragment_transport;
    // xPeerReportMessage report_peer;
    // ----------------------------------------
    xRequestFragmentCreation create_frag;

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

  struct StateReceivedPacket { xPacket packet; int from_fd; } StateReceivedPacket ;

  struct StateRawPackets { 
      uint8_t trigger_pkt; 
      size_t n_pkts; 
      uint64_t total_size; 
      int client_fd; 

      xRequestFileCreation fc; 
      xRequestFragmentCreation fragc;

      char *buffer;  // set by the raw bytes handler
  } StateRawPackets;

  struct StateHandleNewFile { 
      xFileContainer *fc;
      char *buffer; 
  } StateHandleNewFile;
  
  struct StateRequestedFile { 
    xRequestFile f; 
    int from_fd; 
    int file_id; 
    uint64_t file_size; 
    int fragment_count ;
    int fragment_found;
    node_id_t deliver_to;    

    char *buffer;
  } StateRequestedFile;


} uMachineState;

// ------------------------------------------------------------ 
typedef enum  {

    SERVER_BOOTING              = 0,
    SERVER_CONNECTING           = 1,

    //- 
    SERVER_BEGIN_OPERATION      , // branch to role-spefici
                                  //
    // - COMMON
    SERVER_IDLE                 ,
    SERVER_RECEIVED_PACKET,
    SERVER_WAITING_RAW_PACKETS,
    SERVER_RECEIVED_FRAGMENT,

    SERVER_WAIT_REQUEST_FRAGMENTS,

    // iDX SPECIFIC
    SERVER_INDEX_PRESENT_ITSELF,
    SERVER_INDEX_WAITING_PEERS,
    SERVER_INDEX_HANDLE_NEW_FILE,
    // file stuff
        SERVER_INDEX_FANOUT_FRAGMENTS,
        SERVER_INDEX_REQUEST_FRAGMENTS,

    // non-iDX specifics
    SERVER_WAIT_INDEX_GOSSIP,
    SERVER_REPORT_KNOWLEDGE_TO_INDEX,





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
    uint64_t state_changed_at;
    uMachineState machine_state;  

    uint64_t net_size;

    uint64_t death_count;

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
uint64_t server_millis_in_state(Server *sv);

bool server_is_peerf_connected(const Server *sv);
bool server_is_peerb_connected(const Server *sv);



int server_accept(Server *sv);
void server_close_socket(Server *sv, int socket);

int server_dial(Server *sv, Address * a);
int server_dial_index(Server *sv);
int server_dial_peer(Server *sv);


int server_is_index(Server *sv);


void server_index_save_reported_peer(Server *sv, xPacket *packet);


size_t server_send_to_peer_f(Server *sv, xPacket *packet);
size_t server_send_to_index(Server *sv, xPacket *packet);
size_t server_send_to_socket(Server *sv, xPacket *packet, int fd);


int server_send_large_buffer_to( Server *sv, int fd, int buffer_size, char *fragbuffer);
int server_wait_large_buffer_from( Server *sv, int fd, int buffer_size, char *file_buffer );


xPacket server_wait_from_peer_b(Server *sv);
xPacket server_wait_from_socket(Server *sv, int fd);

node_id_t server_wait_client_presentation(Server *sv, int c);


void server_onxpacket(Server *sv);
void server_handle_raw_packets(Server *sv);

int server_send_ok(Server *sv, int to);
int server_send_not_ok(Server *sv, int to);
int server_wait_ok(Server *sv, int to); 


// ------------------------------------------------------------
// XPACKET SHIT
// ------------------------------------------------------------

xPacket xpacket_report_self( Server *sv );
xPacket xpacket_presentation( Server *sv );
xPacket xpacket_ok( Server *sv ); 
xPacket xpacket_not_ok( Server *sv ); 
xPacket xpacket_send_fragment( Server *sv, xRequestFragmentCreation *frag);
xPacket xpacket_request_file_response( Server *sv, xFileContainer *fc);



void xpacket_debug(const xPacket *p);


// ------------------------------------------------------------ 


void xreqfragcreation_new( xRequestFragmentCreation *fragcreation, xFileContainer *fc, xFragmentNetworkPointer *frag );

// ------------------------------------------------------------ 

void print_state(eServerState st);

#endif // SERVER_H