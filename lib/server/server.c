#include "server.h"
    
#include "../tcplib.h"    // your TCP helpers
#include "../defines.h"
#include <stdio.h>
#include <stdlib.h>

// ------------------------------------------------------------
// Initialize server
// ------------------------------------------------------------
int server_init(Server *sv, const Args *opts) {

    if (!sv) return 0;

    // ------------------------------------------------------------ 
    sv->me.node_id = opts->id;
    int r = address_from_string( &sv->me.ip, opts->ip );
    if ( r == 0 ) {
        return 0;
    }
    // ------------------------------------------------------------ 

    // Forward peer
    sv->peer_f.node_id = opts->peer_id;
    r = address_from_string( &sv->peer_f.ip, opts->peer_ip );
    if ( r == 0 ) 
    {
        return 0;
    }
    sv->peer_f.stream_fd = -1;
    sv->peer_f.status.open = false;

    // Backward peer is unknown yet.
    sv->peer_b.node_id = (sv->me.node_id + opts->netsize - 1) % opts->netsize;
    sv->peer_b.stream_fd = -1;
    sv->peer_b.status.open = false;

    // --------------------------------------------------
    sv->state       = SERVER_BOOTING;
    sv->net_size    = opts->netsize;
    sv->peers       = malloc( sizeof(int) * sv->net_size );

    sv->listener_fd = -1;
    sv->client_fd   = -1;

    sv->index_data = NULL;


    return 1;
}

// ------------------------------------------------------------
// Open listener 
// ------------------------------------------------------------
int server_open(Server *sv) {
    if (!sv) return 0;

    int fd = tcp_listen( sv->me.ip.port );
                                                     
    if (fd < 0) {
        perror("tcp_listen");
        return 0;
    }

    sv->listener_fd = fd;

    return 1;
}

// ------------------------------------------------------------
// Close all connections 
// ------------------------------------------------------------
void server_close(Server *sv) {
    if (!sv) return;

    if ( sv->peer_b.stream_fd > 0)
        tcp_close(sv->peer_b.stream_fd);

    if ( sv->peer_f.stream_fd > 0)
        tcp_close(sv->peer_f.stream_fd);

    if (sv->listener_fd > 0)
        tcp_close(sv->listener_fd);

    free(sv);
}

// ------------------------------------------------------------
void server_set_state(Server *sv, eServerState st) {
    
    #if LOG_STATE_CHANGES
        printf("\n---------------------------------\n");
        printf("FROM:\t");
        print_state(sv->state);
        printf("\nTO:  \t");
        print_state(st);
        printf("\n---------------------------------\n");
    #endif

    if (sv) sv->state = st;

    // required state swaps
    switch (st)  {
        case SERVER_INDEX_WAITING_PEERS: {
            sv->machine_state.StateIndexWaitingPeers.connected = 0;
        }
        default: {
            // nothing to do
        }
    }

}
// ------------------------------------------------------------
// Check peer connection states
// ------------------------------------------------------------
bool server_is_peerf_connected(const Server *sv) {
    return sv && sv->peer_f.status.open;
}

bool server_is_peerb_connected(const Server *sv) {
    return sv && sv->peer_b.status.open;
}


int server_dial(Server *sv, Address *a) {
    if (!sv) return 0;

    int fd = tcp_open(a);

    return fd;
}

// ------------------------------------------------------------
// Dial forward peer 
// ------------------------------------------------------------
int server_dial_peer(Server *sv) {
    if (!sv) return 0;
    
    printf("#%ld -> #%ld @ _:%d\n",sv->me.node_id, sv->peer_f.node_id, sv->peer_f.ip.port);

    int fd = tcp_open(&sv->peer_f.ip);

    if (fd < 0) {
        perror("tcp_open");
        return 0;
    }

    sv->peer_f.stream_fd = fd;
    sv->peer_f.status.open = true;
    sv->peer_f.status.tx = 0;
    sv->peer_f.status.rx = 0;

    return 1;
}

// ------------------------------------------------------------
// Accept backward peer 
// ------------------------------------------------------------
int server_accept(Server *sv) {
    if (!sv || sv->listener_fd < 0) return -1;

    tcp_socket client_fd = tcp_accept(sv->listener_fd);

    if (client_fd == NO_CONNECTION_WAITING) {
        return NO_CONNECTION_WAITING;
    }

    if (client_fd < 0) {
        perror("tcp_accept");
        return 0;
    }

    return client_fd;
}

void server_close_socket(Server *sv, int socket) {
    if (!sv || sv->listener_fd < 0) return;
    return tcp_close( socket );
}


// ------------------------------------------------------------
//  Dial Index
// ------------------------------------------------------------
int server_dial_index(Server *sv) {
    if (!sv) return 0;
    
    printf("Calling index...\n");

    int fd = tcp_open(&sv->index.ip);

    if (fd < 0) {
        perror("tcp_open");
        return 0;
    }

    sv->index.stream_fd = fd;
    sv->index.status.open = true;
    sv->index.status.tx = 0;
    sv->index.status.rx = 0;

    return 1;
}

// ------------------------------------------------------------
// ------------------------------------------------------------
size_t server_send_to_peer_f(Server *sv, xPacket *packet) {
    return tcp_send( sv->peer_f.stream_fd, packet->bytes.raw , packet->size );
}
// ------------------------------------------------------------
size_t server_send_to_index(Server *sv, xPacket *packet) {
    return tcp_send( sv->index.stream_fd, packet->bytes.raw , packet->size );
}
// ------------------------------------------------------------
size_t server_send_to_socket(Server *sv, xPacket *packet, int fd) {
    return tcp_send( fd , packet->bytes.raw , packet->size );

}
// ------------------------------------------------------------
int server_send_large_buffer_to( Server *sv, int fd, int buffer_size, char *buffer, int bucket_size )
{
    printf("SENDING LARGE BUFFER\n");
    int n_packets = buffer_size / bucket_size + 1;
    
    xPacket x = {0};
    char* bucket = (char*)&x.bytes.raw;

    int results = 0;

    int curptr = 0;
    for ( int i = 0 ; i < n_packets ; i++ ) 
    {
        printf("SENDING PART %d of %d\n", i+1, n_packets);

		int st  = i * bucket_size; 
		int end = st + bucket_size; 
		if ( end > buffer_size )
        {
			end = buffer_size;
		}

        int size = end - st;

        memcpy( bucket, buffer + curptr, size );
        x.size = size;

        int r = server_send_to_socket(sv, &x, fd);
        
        if ( r <= 0 ) {
            printf("SOMETING STRANGE HAPPENED ON BUCKET #%d\n", i+1);
        }

        results += r;
        curptr += size;

        printf("TOTAL of %d bytes sent.\n", curptr);
    }

    return 1;
}

//-
//
//
//
//
//
//
//

node_id_t server_wait_client_presentation(Server *sv, int c) {
    xPacket p = server_wait_from_socket(sv, c);

    if ( p.size == 0 ) {
        return 0;
    }

    if ( p.bytes.comm.type != TYPE_PRESENT_ITSELF ) {
        return 0;
    }

    return p.bytes.comm.sender_id;
}

xPacket server_wait_from_socket(Server *sv, int fd) {

    xPacket p = {0};

    int read = tcp_recv( fd, p.bytes.raw, sizeof(p.bytes.raw));

    p.size = read;

    if (read < 0) {
        perror("tcp_recv");
        // maybe handle?
    }

    return p;
}

xPacket server_wait_from_peer_b(Server *sv) {

    xPacket p = {0};

    int read = tcp_recv( sv->peer_b.stream_fd, p.bytes.raw, sizeof(p.bytes.raw));

    p.size = read;

    if (read < 0) {
        perror("tcp_recv");
        // maybe handle?
    }

    return p;
}



void server_index_save_reported_peer(Server *sv, xPacket *p) {

    node_id_t sender = p->bytes.comm.sender_id;
    Address peer_addr = p->bytes.comm.content.report_self.peer_addr;

    // @TODO: validate sender_id
    printf(" SAVING NODE #%ld \n", sender);
    sv->index_data->known_peers++;
    *(sv->index_data->peer_ips + sender - 1) = peer_addr;
}

// ------------------------------------------------------------
xPacket xpacket_report_self( Server *sv ) 
{
    xPacket p = {0};

    p.bytes.comm.sender_id  = sv->me.node_id;
    p.bytes.comm.type       = TYPE_REPORT_SELF;

    // xPeerConnection c = sv->peer_f;
    p.bytes.comm.content.report_self.peer_addr  = sv->me.ip;
    // p.bytes.comm.content.report_self.peer_id    = c.node_id;

    p.size = sizeof( p.bytes.comm ) + sizeof(p.size);

    return p;
}
// ------------------------------------------------------------
xPacket xpacket_ok( Server *sv ) 
{
    xPacket p = {0};

    p.bytes.comm.sender_id  = sv->me.node_id;
    p.bytes.comm.type       = TYPE_OK;

    p.size = sizeof( p.bytes.comm ) + sizeof(p.size);

    return p;
}
// ------------------------------------------------------------
xPacket xpacket_presentation( Server *sv ) 
{
    xPacket p = {0};

    p.bytes.comm.sender_id  = sv->me.node_id;
    p.bytes.comm.type       = TYPE_PRESENT_ITSELF;

    p.size = sizeof( p.bytes.comm ) + sizeof(p.size);

    return p;
}
// ------------------------------------------------------------
xPacket xpacket_send_fragment( Server *sv, xRequestFragmentCreation *frag) 
{
    xPacket p = {0};

    p.bytes.comm.sender_id  = sv->me.node_id;
    p.bytes.comm.type       = TYPE_STORE_FRAGMENT;

    p.bytes.comm.content.create_frag = *frag;

    p.size = sizeof( p.bytes.comm ) + sizeof(p.size);

    return p;
}
// ------------------------------------------------------------
void xpacket_debug(const xPacket *p) {
    if (!p) {
        printf("xPacket: (null)\n");
        return;
    }

    printf("xPacket raw[0:%d]: \"", p->size);
    for (int i = 0; i < p->size && i < 4096; ++i) {
        unsigned char c = p->bytes.raw[i];
        printf("\\x%02X", c);
    }
    printf("\"\n");
}







void xreqfragcreation_new( xRequestFragmentCreation *fragcreation, xFileContainer *fc, xFragmentNetworkPointer *frag )
{
    // sets file name
    memcpy(
            fragcreation->file_name,
            fc->file_name,
            sizeof(fc->file_name)
          );

    fragcreation->file_size             = fc->size;
    fragcreation->file_id               = fc->file_id;
    fragcreation->fragment_count_total  = fc->fragment_count_total;
    fragcreation->frag_id               = frag->fragment;
    fragcreation->frag_size             = frag->size;
}










// ------------------------------------------------------------
void print_state(eServerState st) {
    switch (st) {
        case SERVER_BOOTING:
            printf("SERVER_BOOTING");
            break;

        case SERVER_CONNECTING:
            printf("SERVER_CONNECTING");
            break;

        case SERVER_BEGIN_OPERATION:
            printf("SERVER_BEGIN_OPERATION");
            break;

        case SERVER_IDLE:
            printf("SERVER_IDLE");
            break;

        case SERVER_RECEIVED_PACKET:
            printf("SERVER_RECEIVED_PACKET");
            break;

        case SERVER_WAITING_RAW_PACKETS:
            printf("SERVER_WAITING_RAW_PACKETS");
            break;

        case SERVER_RECEIVED_FRAGMENT:
            printf("SERVER_RECEIVED_FRAGMENT");
            break;

        case SERVER_INDEX_PRESENT_ITSELF:
            printf("SERVER_INDEX_PRESENT_ITSELF");
            break;

        case SERVER_INDEX_WAITING_PEERS:
            printf("SERVER_INDEX_WAITING_PEERS");
            break;

        case SERVER_INDEX_HANDLE_NEW_FILE:
            printf("SERVER_INDEX_HANDLE_NEW_FILE");
            break;

        case SERVER_INDEX_FANOUT_FRAGMENTS:
            printf("SERVER_INDEX_FANOUT_FRAGMENTS");
            break;

        case SERVER_WAIT_INDEX_GOSSIP:
            printf("SERVER_WAIT_INDEX_GOSSIP");
            break;

        case SERVER_REPORT_KNOWLEDGE_TO_INDEX:
            printf("SERVER_REPORT_KNOWLEDGE_TO_INDEX");
            break;

        case SERVER_OTHER:
            printf("SERVER_OTHER");
            break;

        default:
            printf("UNKNOWN_STATE (%d)", st);
            break;
    }
}
