#include "lib/fileserver/fs.h"
#include "lib/server/server.h"
#include "lib/tcplib.h"
#include "lib/args.h"
#include "lib/nettypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lib/defines.h"



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
    
    Server sv; 

    xFileServer fs;
    xFileNetworkIndex fnetidx; // used only by the index
    
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

    if ( ! xfilenetindex_init(&fnetidx) )
    {
        perror("Unable to initalize file network index server.");
        return 1;
    }

    #if DEBUG
        const char frag1[] = "Hello ";
        const char frag2[] = "World! i'm dumb";
        uint64_t sz = sizeof(frag1) +sizeof(frag2);

        xFileContainer *file = xfileserver_add_file(&fs, "data.bin", 1, sz, 2);

        xfileserver_add_fragment(file, 1, frag1, sizeof(frag1));
        xfileserver_add_fragment(file, 2, frag2, sizeof(frag2));

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

            if (p.bytes.comm.type != TYPE_REPORT_SELF)
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

            xPacket p = xpacket_report_self(&sv);
            // xpacket_debug(&p);

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

            //printf(".");

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
                    xPacket pkt_ok = xpacket_ok(&sv);
                    server_send_to_socket( &sv, &pkt_ok, c );  

                    if (N == CLIENT_NODE_ID)
                    {
                        printf("OMG! The user <3 \n");
                        sv.client_fd = c;
                        break;
                    }
                    

                    printf("Waiting...\n");
                    xPacket p = server_wait_from_socket(&sv, c);

                    if (p.size == 0)
                    {
                        printf("prolly closed by peer.\n");
                        break;
                    }

                    printf("RECEIVED TYPE %d\n", p.bytes.comm.type);

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
                sv.machine_state.StateRawPackets.trigger_pkt = TYPE_CREATE_FILE;
                sv.machine_state.StateRawPackets.fc = fc;
                sv.machine_state.StateRawPackets.n_pkts = (fc.file_size / 4096) + 1;
                sv.machine_state.StateRawPackets.total_size = fc.file_size;
                sv.machine_state.StateRawPackets.client_fd = sv.machine_state.StateReceivedPacket.from_fd;
                server_set_state(&sv, SERVER_WAITING_RAW_PACKETS);

                break;
            }
            case TYPE_STORE_FRAGMENT:
            {
                xRequestFragmentCreation fragc = p.bytes.comm.content.create_frag;

                printf("FILE NAME: \t %s\n", fragc.file_name);
                printf("FRAG ID: \t %ld\n", fragc.frag_id);
                printf("FRAG SIZE: \t %ld\n", fragc.frag_size );
                
                xFileContainer *f = xfileserver_find_file(&fs, fragc.file_id);
                if ( f == NULL ) 
                { // must create file container
                    f = xfileserver_add_file( &fs,  fragc.file_name, fragc.file_id, fragc.file_size, fragc.fragment_count_total);
                    printf("FILE CREATED \n");
                }    

                // uau
                sv.machine_state.StateRawPackets.trigger_pkt = TYPE_STORE_FRAGMENT;
                sv.machine_state.StateRawPackets.fragc = fragc;
                sv.machine_state.StateRawPackets.n_pkts = (fragc.frag_size/ 4096) + 1;
                sv.machine_state.StateRawPackets.total_size = fragc.frag_size;
                sv.machine_state.StateRawPackets.client_fd = sv.machine_state.StateReceivedPacket.from_fd;

                server_set_state(&sv, SERVER_WAITING_RAW_PACKETS);

                // send a ok to signal it is ready
                xPacket pok = xpacket_ok(&sv);
                server_send_to_socket( &sv, &pok, sv.machine_state.StateReceivedPacket.from_fd );

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
            uint8_t trigger = sv.machine_state.StateRawPackets.trigger_pkt;

            char *file_buffer = (char *)malloc(sizeof(char) * size);

            sv.machine_state.StateRawPackets.buffer = file_buffer;

            xPacket xx = {0};
            xx.bytes.comm.sender_id = sv.me.node_id;
            xx.bytes.comm.content.report_self.peer_addr = sv.peer_f.ip;
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

                if ( trigger == TYPE_CREATE_FILE && sv.index.node_id != sv.me.node_id )
                {
                    printf("SINCRONIZANDO INDEX.\n");
                    server_send_to_index( &sv, &p );
                }

                printf("RAW : %.2f%% bytes.\n", (100 * (float)populated / (float)size));
            }

            printf("DONE\n");
            switch (trigger) {
            case TYPE_CREATE_FILE: 
            {
                if (sv.index.node_id == sv.me.node_id)
                {
                    server_set_state(&sv, SERVER_INDEX_HANDLE_NEW_FILE);
                    break;
                }

                server_set_state(&sv, SERVER_IDLE);
                break;
            }
            case TYPE_STORE_FRAGMENT:
            {
                #if LOG_BUFFERS
                    printf("-RAW BUFFER---------\n");
                    printf("%s", sv.machine_state.StateRawPackets.buffer);
                    printf("--------------------\n");
                #endif

                // TODO: goto handle received fragment
                // server_set_state(&sv, SERVER_IDLE);
                server_set_state(&sv, SERVER_RECEIVED_FRAGMENT);
                break;
            }
            }    


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
             
            if ( sz <= sv.net_size || sz <= MINIMAL_SIZE_FOR_SPLIT ) 
            {
                printf("FILE TOO SMALL TO SPLIT.\n");
                fragcount = 1;
            }

            int id = ++FILE_SERVER_ID;
            xFileContainer *file = xfileserver_add_file(&fs, fc.name, id, sz, fragcount);
   

            printf("INDEXING FILE...\n");
            xFileInNetwork *f = xfilenetindex_new_file(id, fragcount * REDUNDANCY);

            int fragsz = sz / fragcount;
            int remain = sz % fragcount;
    
            int internaloffset = 0;

            for ( int i = 0; i < fragcount; i++ ) 
            {
                int fragmentsz = (i+1) == fragcount ? fragsz + remain : fragsz;
                   
                while (1) {
                    // known peers is NET_SIZE - 1 (this one is myself)
                    if ( (i + internaloffset) > (sv.index_data->known_peers) ) {
                        // this is an inconsistency, netsize is less than active peers;
                        goto weird;
                    }

                    Address *a = sv.index_data->peer_ips + i + internaloffset;

                    if ( a == NULL ) {
                        printf("INCREASING OFFSET\n");
                        internaloffset++;
                        continue;
                    }
                    break;
                }

                int node = i + internaloffset;

                for (int j = 0; j < REDUNDANCY; j++) 
                {
                    node_id_t nid = (node + j) % sv.net_size;
                    nid += 1; // nodes starts at 1
                              
                    printf("Fragment #%d into node %ld\n", i, nid);
                    f->fragments[i * REDUNDANCY + j].fragment    = i + 1;
                    f->fragments[i * REDUNDANCY + j].size        = fragmentsz;
                    f->fragments[i * REDUNDANCY + j].node_id     = nid;
                }

                printf("Fragment #%d size %d\n", i, fragmentsz);
            }

            /**
             * Adds to the index, will be used later to fan out 
             * segments
             */
            xfilenetindex_add_file(&fnetidx, f);

            xfileserver_debug(&fs);

            server_set_state(&sv, SERVER_INDEX_FANOUT_FRAGMENTS);
            sv.machine_state.StateHandleNewFile.fc      = file;
            sv.machine_state.StateHandleNewFile.buffer  = b;
            break;
weird:
            printf("SOMETHING REALLY WEIRD JUST HAPPENED.\n");
            server_set_state(&sv, SERVER_IDLE);
            break;
        }

        case SERVER_INDEX_FANOUT_FRAGMENTS:
        {
            printf("PREPARING TO FAN OUT\n");
            xfilenetindex_debug(&fnetidx);

            /**
             * file buffer read from socket 
             */
            char *buffer = sv.machine_state.StateHandleNewFile.buffer;

            xFileContainer *fc = sv.machine_state.StateHandleNewFile.fc;
            xFileInNetwork *f = xfilenetindex_find_file(&fnetidx, fc->file_id);

            printf("FILE name=%s size=%d fragments=%d.\n", fc->file_name, fc->size, f->total_fragments);
          
            #if LOG_BUFFERS
                printf("--------------------\n");
                printf("%s", buffer);
                printf("--------------------\n");
            #endif
            

            size_t bufptr = 0;
            int baseoffset = fc->size / (f->total_fragments / REDUNDANCY ); 

            for ( int i = 0 ; i < f->total_fragments ; i++ )
            {
                xFragmentNetworkPointer frag = f->fragments[i];
                int offset =  baseoffset * (frag.fragment - 1);

                printf("FRAG #%d SIZE=%d OFFSET=%d\n", frag.fragment, frag.size, offset);

                // IF IS MY FRAGMENT
                if ( frag.node_id == sv.me.node_id ) 
                {
                    printf("OHH THIS ONE IS MINE...\n");

                    char *buf = (char*) malloc( sizeof(char) * frag.size );
                    memcpy(buf, buffer + offset, frag.size);

                    // special
                    if ( fc->fragments[0].fragment_id == 0 ) {
                        printf("INTO POS 0\n");
                        fc->fragments[0].fragment_id    = frag.fragment; 
                        fc->fragments[0].fragment_size  = frag.size; 
                        fc->fragments[0].fragment_bytes = buf;
                    } else 
                    {
                        printf("INTO POS 1\n");
                        fc->fragments[1].fragment_id    = frag.fragment; 
                        fc->fragments[1].fragment_size  = frag.size; 
                        fc->fragments[1].fragment_bytes = buf;
                    }
                } else 
                {
                    Address *a = sv.index_data->peer_ips + frag.node_id - 1; // nodes start at index 1
                    if (a == NULL) {
                        printf("NODE #%ld address is NULL", frag.node_id);
                        // TODO: falhar
                        continue;
                    }

                    printf("DIALING :%d...\n", a->port);                

                    int fd = server_dial(&sv, a);
                    xPacket p = xpacket_presentation(&sv);

                    server_send_to_socket(&sv, &p, fd);

                    xPacket read = server_wait_from_socket(&sv, fd);
                    printf("RECEIVED RESPONSE %d\n", read.bytes.comm.type);                

                     
                    xRequestFragmentCreation fragcreation = {0};
                    xreqfragcreation_new(&fragcreation, fc, &frag);

                    xPacket pkt_fragment = xpacket_send_fragment(&sv, &fragcreation);

                    printf("SENDING to :%d\n", a->port);                

                    server_send_to_socket(&sv, &pkt_fragment, fd);
                    read = server_wait_from_socket(&sv, fd);

                    printf("RECEIVED RESPONSE %d\n", read.bytes.comm.type);                
                    printf("STARTING RAW TRANSMISSION: %d\n", read.bytes.comm.type);                
                    
                    char *fragbuffer = buffer + offset; // frag id starts at 1
                    
                    #if LOG_BUFFERS
                        printf("-FRAG %3d--------\n", frag.fragment);
                        printf("%.*s\n", frag.size, fragbuffer);
                        printf("--------------------\n");
                    #endif

                    server_send_large_buffer_to( &sv, fd, frag.size, fragbuffer, 4096 );
                }
            }

            xfileserver_debug(&fs);

            server_set_state( &sv, SERVER_IDLE);
            break;
        }

        case SERVER_RECEIVED_FRAGMENT:
        {
            printf("I RECEIVED A FRAGMENT\n");

            xRequestFragmentCreation c  = sv.machine_state.StateRawPackets.fragc;
            xFileContainer *fc          = xfileserver_find_file(&fs, c.file_id);
            char *buffer                = sv.machine_state.StateRawPackets.buffer;

            // this is a problem
            if (fc == NULL) {
                printf("FILE IS UNKNOWN....\n");
                server_set_state(&sv, SERVER_IDLE);
                break;
            }

            printf("OK. FRAG #%ld of FILE %s\n", c.frag_id, c.file_name);

            #if LOG_BUFFERS
                printf("--------------------\n");
                printf("%s\n", buffer);
                printf("--------------------\n");
            #endif
            
            eFileAddFragStatus f = xfileserver_add_fragment(fc, c.frag_id, buffer, c.frag_size);
            if ( f == FRAG_OK ) 
            {
                printf("FRAGMENT INCLUDED SUCCESFULLY.\b");
            } else
            {
                printf("ERROR INCLUDING FRAGMENT");
            }

            xfileserver_debug(&fs);

            free(buffer); //this clears from state also, so it should not be used anymore.
                          // TODO: clear whole machine
            server_set_state(&sv, SERVER_IDLE);
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

