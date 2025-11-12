
#include "server.h"

#include "../tcplib.h"    // your TCP helpers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    sv->peers     = malloc( sizeof(int) * sv->net_size );
    sv->listener_fd = -1;

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
//-


xPacket server_wait_from_client(Server *sv, int fd) {

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
    node_id_t peerid = p->bytes.comm.content.report_peer.peer_id;
    Address peer_addr = p->bytes.comm.content.report_peer.peer_addr;

    // @TODO: validate sender_id
    printf(" SAVING NODE #%ld \n", sender);
    sv->index_data->known_peers++;
    *(sv->index_data->peer_ips + sender - 1) = peer_addr;

    printf("FOUND:\n");
    for (int i = 0; i < sv->net_size - 1; i++)
    {
        Address a = *(sv->index_data->peer_ips + i);
        printf("NODE #%d -> :%d\n", i + 1, a.port);
    }
}

// ------------------------------------------------------------
xPacket xpacket_report_peer( Server *sv ) 
{
    xPacket p = {0};

    p.bytes.comm.sender_id  = sv->me.node_id;
    p.bytes.comm.type       = TYPE_REPORT_PEER;

    xPeerConnection c = sv->peer_f;
    p.bytes.comm.content.report_peer.peer_addr  = c.ip;
    p.bytes.comm.content.report_peer.peer_id    = c.node_id;

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