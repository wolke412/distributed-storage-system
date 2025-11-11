// example_server.c
#include "lib/server/server.h"
#include "lib/tcplib.h"
#include "lib/args.h"
#include "lib/nettypes.h"
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("Booting...");

    // ------------------------------------------------------------ 
    Args args;
    if (! parse_args(argc, argv, &args) ) 
    {
        perror("Unable to parse args.");
        return 1;
    }
    debug_args_inline(&args);
    // ------------------------------------------------------------ 

    // ------------------------------------------------------------ 
    Address addr;
    if ( ! address_from_string( &addr, args.ip ) ) 
    {
        return 1;
    }
    debug_address(&addr);
    // ------------------------------------------------------------ 
    
    Server sv; 
    
    if ( ! server_init(&sv, &args) )
    {
        perror("Unable to initalize server.");
        return 1;
    }

    printf("Server initialized. Opening socket... \n");


    while(1) 
    {
        switch (sv.state) {
            case SERVER_BOOTING: {

                if ( ! server_open(&sv) ) return 1;

                printf("Server listening on :%d...\n", sv.me.ip.port);
                server_set_state(&sv, SERVER_CONNECTING);
                break;
            }

            case SERVER_CONNECTING: {
                if ( ! server_is_peerb_connected(&sv) )
                {
                    tcp_socket client = server_accept(&sv);
                    if ( client > 0 ) 
                    {
                        sv.peer_b.status.open = true;
                        sv.peer_b.stream_fd = client; 
                    }
                }

                if ( ! server_is_peerf_connected(&sv) )
                {
                    tcp_socket client = server_dial_peer(&sv);
                    if ( !client ) break;
                }
                
                // if both connected, can proceed operations
                if (server_is_peerf_connected(&sv) && server_is_peerb_connected(&sv))
                {
                    server_set_state(&sv, SERVER_BEGIN_OPERATION);
                }

                break; 
            }

            case SERVER_BEGIN_OPERATION:  {
                // if i'm the index i must assume leadership
                if ( sv.net_size == sv.me.node_id ) {

                    sv.index_data           = malloc( sizeof(xIndexData) );
                    sv.index_data->peer_ips = malloc( sv.net_size * sizeof( Address ) );

                    server_set_state(&sv, SERVER_INDEX_PRESENT_ITSELF);
                }
                else {
                    server_set_state(&sv, SERVER_WAIT_INDEX_GOSSIP);
                }

                break;
            }

            // ------------------------------------------------------------
            //     INDEX STUFF
            // ------------------------------------------------------------
            case SERVER_INDEX_PRESENT_ITSELF: {

                xPacket p = {0};

                p.bytes.index_presentation_pkt.sender_id  = sv.me.node_id;
                p.bytes.index_presentation_pkt.index_id   = sv.me.node_id;
                p.bytes.index_presentation_pkt.index_addr = sv.me.ip;

                p.size = sizeof(p.bytes.index_presentation_pkt);

                int w = server_send_to_peer_f( &sv, &p );

                printf("Wrote %d bytes to PEER #%ld\n", w, sv.peer_f.node_id);

                if ( w <= 0) {
                    printf("Algo estranho rolou... Incapaz de se autoproclamar.\n");
                    return 1;
                }

                server_set_state(&sv, SERVER_INDEX_WAITING_PEERS);

                break;
            }

            case SERVER_INDEX_WAITING_PEERS: {

                tcp_socket c = server_accept(&sv);
                if ( c < 0 ) break;

                sv.machine_state.StateIndexWaitingPeers.connected++;

                printf("RECEIVED %d / %d \n", sv.machine_state.StateIndexWaitingPeers.connected, sv.net_size - 1);

                server_close_socket( &sv, c );

                if ( sv.machine_state.StateIndexWaitingPeers.connected == sv.net_size - 1) {
                    server_set_state(&sv, SERVER_IDLE);
                }

                break;
            }


            // ------------------------------------------------------------
            //     NON_INDEX STUFF
            // ------------------------------------------------------------
            case SERVER_WAIT_INDEX_GOSSIP: {

                xPacket p = server_wait_from_peer_b(&sv);

                // ----------------------------------------
                xIndexPresentationPacket p2 = p.bytes.index_presentation_pkt;
                char addr[40];
                address_to_string( &p2.index_addr, addr, 40);

                printf("SO INDEX IS #%ld @ %s \n", p2.index_id, addr);
                sv.index.ip = p2.index_addr;  
               
                // forwarding                
                if ( p2.index_id != sv.peer_f.node_id ) {

                    // set my own id
                    p.bytes.index_presentation_pkt.sender_id = sv.me.node_id; 

                    int w = server_send_to_peer_f( &sv, &p );

                    printf("Wrote %d bytes to PEER #%ld\n", w, sv.peer_f.node_id);

                    if ( w <= 0) {
                        printf("Algo estranho rolou... Incapaz de encaminahr informação.\n");
                        return 1;
                    }
                }

                server_set_state(&sv, SERVER_REPORT_KNOWLEDGE_TO_INDEX);

                break;
            }

            
            case SERVER_REPORT_KNOWLEDGE_TO_INDEX: {
                
                if ( ! server_dial_index( &sv ) ) {
                    printf("Error connecting to index.\n");
                    break;
                }

                server_set_state(&sv, SERVER_IDLE);

                break;

            }
            
            case SERVER_IDLE: {
                printf("IDLING...\n");
            }




            default : { break; }
        }

        usleep(10 * 1000 * 30);


    }
   
    server_close(&sv);
    return 0;
}

