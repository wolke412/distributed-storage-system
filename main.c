#include "lib/fileserver/fs.h"
#include "lib/server/server.h"
#include "lib/tcplib.h"
#include "lib/args.h"
#include "lib/nettypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define DEBUG               1



int main(int argc, char **argv) {

    // print shit
    setvbuf(stdout, NULL, _IONBF, 0);

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
    
    xFileServer fs;
    xFileIndex  fi;
    Server sv; 
    
    if ( ! server_init(&sv, &args) )
    {
        perror("Unable to initalize server.");
        return 1;
    }

    if ( ! xfileserver_init(&fs) )
    {
        perror("Unable to initalize file server.");
        return 1;
    }

    #if DEBUG
        const char frag1[] = "Hello ";
        const char frag2[] = "World! i'm very gay";
        uint64_t sz = sizeof(frag1) +sizeof(frag2);

        xFileContainer *file = xfileserver_add_file(&fs, "data.bin", 1, sz, 2);

        xfileserver_add_fragment(file, 0, frag1, sizeof(frag1));
        xfileserver_add_fragment(file, 1, frag2, sizeof(frag2));

        xfileserver_debug(&fs);
    #endif

    printf("Server initialized. Opening socket... \n");

    while(1) 
    {
        switch (sv.state) {
        case SERVER_BOOTING:
        {

            if (!server_open(&sv))
                return 1;

            printf("Server listening on :%d...\n", sv.me.ip.port);
            server_set_state(&sv, SERVER_CONNECTING);
            break;
        }

        case SERVER_CONNECTING:
        {
            if (!server_is_peerb_connected(&sv))
            {
                tcp_socket client = server_accept(&sv);
                if (client > 0)
                {
                    sv.peer_b.status.open = true;
                    sv.peer_b.stream_fd = client;
                }
            }

            if (!server_is_peerf_connected(&sv))
            {
                tcp_socket client = server_dial_peer(&sv);
                if (!client)
                    break;
            }

            // if both connected, can proceed operations
            if (server_is_peerf_connected(&sv) && server_is_peerb_connected(&sv))
            {
                server_set_state(&sv, SERVER_BEGIN_OPERATION);
            }

            break;
        }

        case SERVER_BEGIN_OPERATION:
        {
            // if i'm the index i must assume leadership
            if (sv.net_size == sv.me.node_id)
            {

                sv.index_data = malloc(sizeof(xIndexData));
                sv.index_data->peer_ips = malloc(sv.net_size * sizeof(Address));

                server_set_state(&sv, SERVER_INDEX_PRESENT_ITSELF);
            }
            else
            {
                server_set_state(&sv, SERVER_WAIT_INDEX_GOSSIP);
            }

            break;
        }

        // ------------------------------------------------------------
        //     INDEX STUFF
        // ------------------------------------------------------------
        case SERVER_INDEX_PRESENT_ITSELF:
        {
            sv.index.node_id = sv.me.node_id;

            xPacket p = {0};

            p.bytes.index_presentation_pkt.sender_id = sv.me.node_id;
            p.bytes.index_presentation_pkt.index_id = sv.me.node_id;
            p.bytes.index_presentation_pkt.index_addr = sv.me.ip;

            p.size = sizeof(p.bytes.index_presentation_pkt);

            int w = server_send_to_peer_f(&sv, &p);

            printf("Wrote %d bytes to PEER #%ld\n", w, sv.peer_f.node_id);

            if (w <= 0)
            {
                printf("Algo estranho rolou... Incapaz de se autoproclamar.\n");
                return 1;
            }

            server_set_state(&sv, SERVER_INDEX_WAITING_PEERS);

            break;
        }

        case SERVER_INDEX_WAITING_PEERS:
        {

            tcp_socket c = server_accept(&sv);
            if (c < 0)
                break;

            // printf("Waiting...\n");

            xPacket p = server_wait_from_socket(&sv, c);

            if (p.size < 0)
            {
                printf("DEU MERDA RECEBENDO CONHECIMENTO EM. \n");
                break;
            }

            if (p.bytes.comm.type != TYPE_REPORT_PEER)
            {
                printf("UNEXPECTED TYPE \n");
                break;
            }

            if (sv.index_data == NULL)
            {
                printf("INDEX STRUTURES NOT BUILT! \n");
                break;
            }

            server_index_save_reported_peer(&sv, &p);

            sv.machine_state.StateIndexWaitingPeers.connected++;

            printf("RECEIVED %ld / %ld \n", sv.machine_state.StateIndexWaitingPeers.connected, sv.net_size - 1);

            server_close_socket(&sv, c);

            if (sv.machine_state.StateIndexWaitingPeers.connected == sv.net_size - 1)
            {
                server_set_state(&sv, SERVER_IDLE);
                printf("FOUND:\n");
                for (int i = 0; i < sv.net_size - 1; i++)
                {
                    Address a = *(sv.index_data->peer_ips + i);
                    printf("NODE #%d -> :%d\n", i + 1, a.port);
                }
            }

            break;
        }

        // ------------------------------------------------------------
        //     NON_INDEX STUFF
        // ------------------------------------------------------------
        case SERVER_WAIT_INDEX_GOSSIP:
        {

            xPacket p = server_wait_from_peer_b(&sv);

            // ----------------------------------------
            xIndexPresentationPacket p2 = p.bytes.index_presentation_pkt;
            char addr[40];
            address_to_string(&p2.index_addr, addr, 40);

            printf("SO INDEX IS NODE #%ld @ %s \n", p2.index_id, addr);
            sv.index.ip = p2.index_addr;
            sv.index.node_id = p2.index_id;

            // forwarding
            if (p2.index_id != sv.peer_f.node_id)
            {

                // set my own id
                p.bytes.index_presentation_pkt.sender_id = sv.me.node_id;

                int w = server_send_to_peer_f(&sv, &p);

                printf("Wrote %d bytes to PEER #%ld\n", w, sv.peer_f.node_id);

                if (w <= 0)
                {
                    printf("Algo estranho rolou... Incapaz de encaminahr informação.\n");
                    return 1;
                }
            }

            server_set_state(&sv, SERVER_REPORT_KNOWLEDGE_TO_INDEX);

            break;
        }

        case SERVER_REPORT_KNOWLEDGE_TO_INDEX:
        {

            if (!server_dial_index(&sv))
            {
                printf("Error connecting to index.\n");
                break;
            }

            printf("INDEX CONNECTION %d\n", sv.index.stream_fd);

            xPacket p = xpacket_report_peer(&sv);
            xpacket_debug(&p);

            int w = 0;
            do
            {
                w = server_send_to_index(&sv, &p);
                perror("index write");
                usleep(10 * 1000);
            } while (w == 0);

            printf("\nWrote %d bytes to INDEX #%ld\n", w, sv.index.node_id);

            server_set_state(&sv, SERVER_IDLE);

            break;
        }

        case SERVER_IDLE:
        {

            printf(".");

            if (sv.client_fd > 0)
            { // client is connected
                bool h = FD_tcp_has_data(sv.client_fd);

                if (h)
                {
                    xPacket p = server_wait_from_socket(&sv, sv.client_fd);

                    if (p.size == 0)
                    {
                        printf("prolly closed by peer.\n");
                        sv.client_fd = 0;
                        break;
                    }

                    server_set_state(&sv, SERVER_RECEIVED_PACKET);
                    sv.machine_state.StateReceivedPacket.from_fd = sv.client_fd;
                    sv.machine_state.StateReceivedPacket.packet = p;

                    break;
                }
            }

            tcp_socket c = server_accept(&sv);

            if (c > 0)
            {
                printf("new connection... waiting identification.\n");

                node_id_t N = server_wait_client_presentation(&sv, c);

                if (!N)
                {
                    printf("FAILED PRESENTATION PROTOCOL.\n");
                    tcp_close(c);
                    break;
                }
                else
                {
                    printf("PRESENTED AS NODE #%lu.\n", N);

                    if (N == CLIENT_NODE_ID)
                    {
                        printf("OMG! The user <3 \n");
                        sv.client_fd = c;
                        break;
                    }

                    xPacket p = server_wait_from_socket(&sv, c);

                    if (p.size == 0)
                    {
                        printf("prolly closed by peer.\n");
                        break;
                    }

                    server_set_state(&sv, SERVER_RECEIVED_PACKET);
                    sv.machine_state.StateReceivedPacket.from_fd = c; 
                    sv.machine_state.StateReceivedPacket.packet = p;

                    break;

                }

            }

            break;
        }

        /**
         *   When any node receives any packet from any peer
         *  ------------------------------------------------------------
         */
        case SERVER_RECEIVED_PACKET:
        {
            xPacket p = sv.machine_state.StateReceivedPacket.packet;
            printf("RECEIVED NEW PACKET OF TYPE=%d\n", p.bytes.comm.type);
            switch (p.bytes.comm.type)
            {
            case TYPE_CREATE_FILE:
            {
                xRequestFileCreation fc = p.bytes.comm.content.create_file;
                printf("\nFILE NAME: \t %s\n", fc.name);
                printf("FILE SIZE: \t %ld \n", fc.file_size);

                if (sv.index.node_id != sv.me.node_id) {
                    printf("SINCRONIZANDO INDEX.\n");
                    while( server_dial_index(&sv) == 0)
                    {
                        usleep(10 * 1000);
                    }

                    xPacket presentation = xpacket_presentation(&sv);

                    int r = server_send_to_index(&sv, &presentation);

                    xpacket_debug(&p);
                    r = server_send_to_index(&sv, &p);
                }
                // uau
                sv.machine_state.StateRawPackets.uploading = true;
                sv.machine_state.StateRawPackets.fc = fc;
                sv.machine_state.StateRawPackets.n_pkts = (fc.file_size / 4096) + 1;
                sv.machine_state.StateRawPackets.total_size = fc.file_size;
                sv.machine_state.StateRawPackets.client_fd = sv.machine_state.StateReceivedPacket.from_fd;
                server_set_state(&sv, SERVER_WAITING_RAW_PACKETS);

                break;
            }

            default:
            { // we ball
                puts((char *)&p.bytes.raw);
                printf("\n");

                break;
            }
            }

            break;
        }

        case SERVER_WAITING_RAW_PACKETS:
        {
            int size = sv.machine_state.StateRawPackets.total_size;
            int n = sv.machine_state.StateRawPackets.n_pkts;
            int c = sv.machine_state.StateRawPackets.client_fd;

            char *file_buffer = (char *)malloc(sizeof(char) * size);

            sv.machine_state.StateRawPackets.buffer = file_buffer;

            xPacket xx = {0};
            xx.bytes.comm.sender_id = sv.me.node_id;
            xx.bytes.comm.content.report_peer.peer_addr = sv.peer_f.ip;
            xx.size = sizeof(xx.bytes.comm);

            usleep(1 * 1000);

            printf("WAITING RAW PAKCETS\n");
            printf("size=%d n=%d client=%d\n", size, n, c);

            int populated = 0;
            for (int i = 0; i < n; i++)
            {
                xPacket p = server_wait_from_socket(&sv, c);

                memcpy(file_buffer + populated, p.bytes.raw, p.size);

                populated += p.size;

                if (sv.index.node_id != sv.me.node_id)
                {
                    printf("SINCRONIZANDO INDEX.\n");
                    server_send_to_index( &sv, &p );
                }

                printf("RAW : %.1f bytes.\n", (populated / size));
            }

            printf("DONE\n");

            if (sv.index.node_id == sv.me.node_id)
            {
                server_set_state(&sv, SERVER_INDEX_HANDLE_NEW_FILE);
                break;
            }

            server_set_state(&sv, SERVER_IDLE);

            break;
        }



        /**
         *   
         *  ------------------------------------------------------------
         */
        case SERVER_INDEX_HANDLE_NEW_FILE:
        {
            xRequestFileCreation fc = sv.machine_state.StateRawPackets.fc;
            char *b     = sv.machine_state.StateRawPackets.buffer;
            uint64_t sz = sv.machine_state.StateRawPackets.total_size;

            printf("HANDLING A %ld FILE named %s... \n", sz, fc.name);

            int fragcount = sv.net_size;
            if ( sz <= sv.net_size || sz <= 1000 ) 
            {
                printf("FILE TOO SMALL TO TRAFEGATE.");
                fragcount = 1;
            }

            xfileserver_add_file(&fs, fc.name, FILE_SERVER_ID++, sz, fragcount);

            int fragsz = sz / fragcount;
            int remain = sz % fragcount;

            for ( int i = 0; i < fragcount; i++ ) 
            {
                int sz = (i+1) == fragcount ? fragsz + remain : fragsz;
                printf("Fragment #%d size %d\n", i, sz);
            }

            xfileserver_debug(&fs);

            // TODO MUST GO TO SERVER_INDEX_FANOUT_FRAGMENTS
            server_set_state(&sv, SERVER_IDLE);
            // .... STATE SHIT
            break;
        }

        default:
        {
            break;
        }
        }

        usleep( 1 * 1000 );
    }



   
    server_close(&sv);
    return 0;
}

