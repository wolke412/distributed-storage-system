#include "statemachine.h"
#include "../tcplib.h"


void xprocedure_check_peer_b(Server *sv, xFileInNetwork *fni) 
{

  xPacket p = { 0 };

  // printf("CHECKING PEER B \n"); 
  int n = tcp_recv_u( sv->peer_b.stream_fd, p.bytes.raw, 4096);
  if ( n <= 0 ) return;

  // since i openly am not abstracting this packet read
  // i must do this stuff
  p.size = n;

  printf("RECEIVED PACKET FROM PEER B | SIZE = %d \n", n); 

  switch( p.bytes.comm.type )
    {
    case TYPE_PEER_DIED: {

      node_id_t dead_id   = p.bytes.comm.content.peer_died.peer_id;
      Address widow       = p.bytes.comm.content.peer_died.sender_address;

      printf("OH, THERE's A DEAD PEER. RIP. NODE-ID=%d\n", dead_id );

      sv->net_size--;
      sv->death_count++;

      /**
       * 
       */
      if ( sv->peer_f.node_id == dead_id ) {
        printf("OH NO, ITS MY NEIGHBOUR!\n");

        sv->peer_f.status.open = false;
        sv->peer_f.ip = widow;
        sv->peer_f.node_id = (sv->me.node_id + 1) % sv->net_size; 

        if ( server_dial_peer(sv) )
        {
          printf("SUCCESSFULLY ESTABLISHED NEW PEER. \n");
        }
      }
      else 
      {
        xprocedure_peer_died_forward(sv, &p);
      }

      /**
       *  INDEX MUST DELETE MAPPED FILE FRAGMENTS.
       * ------------------------------------------------------------
       */
      if ( server_is_index(sv) )
      {
        printf("I MUST UPDATE MY INTERNAL INDEX STUFF \n");
        
        // sv->index_data->known_peers--;

        // keeps known peers to be able to navigtate what once was
        // the full list with nulls in it.
        int i = p.bytes.comm.content.peer_died.peer_id - 1;
        memset(&sv->index_data->peer_ips[i], 0, sizeof(Address));
      }
      else {
        if (dead_id == sv->index.node_id)
        {
          printf("OMG! THE INDEX DIED...\n");
          server_set_state(sv, SERVER_BEGIN_OPERATION);
        }
      }
    }
  }
}

void server_healthcheck( Server *sv )
{
  if ( server_is_peerb_connected( sv ) ) {
    char buf;
    size_t n = tcp_peek_u( sv->peer_b.stream_fd , buf, 1); // peeking zero bytes

    if (n == 0) {
      printf("[HEALTHCHECK] : PEER IS DEAD. RIP. \n");

      sv->peer_b.status.open = false;

      sv->net_size--;
      sv->death_count++;

      xprocedure_peer_died_notify(sv);

      if (server_is_index(sv))
      {
        printf("I MUST UPDATE MY INTERNAL INDEX STUFF \n");

        int i = sv->peer_b.node_id - 1;
        memset(&sv->index_data->peer_ips[i], 0, sizeof(Address));
      }
      server_set_state(sv, SERVER_WAITING_NEW_PEER);
    } else if (n < 0) {
        // fuck it then
    } else {
        // Data is available, connection still open
    }
  }
}

/**
 * 
 * ------------------------------------------------------------
 *  Notes: 
 *      this procedures closes the connection if failed protocol
 */
node_id_t xprocedure_wait_identification(Server *sv, int client) 
{
  node_id_t N = server_wait_client_presentation(&sv, client);

  if (!N) {
    tcp_close(client);
    return 0;
  }

  xPacket pkt_ok = xpacket_ok(&sv);
  size_t sent = server_send_to_socket(&sv, &pkt_ok, client);

  if ( sent <= 0 ) 
  {
    tcp_close(client);
    return 0;
  }

  return N;
}


int xprocedure_send_request_fragment( Server *sv, Address *to , int file_id, int fragment_id, Address *deliver_to )
{
  printf("CONNECTING TO :%d\n", to->port);

  int fd = server_dial(sv, to);
  if (fd <= 0)
  {
    return fd;
  }

  xPacket presentation = xpacket_presentation(sv);
  server_send_to_socket(sv, &presentation, fd);
  if ( ! server_wait_ok( sv, fd ) ) {
    printf("FRAGMENT REFUSED.\n");
    return -1;
  }

  xPacket pkt = {0};

  pkt.bytes.comm.sender_id = sv->me.node_id;
  pkt.bytes.comm.type       = TYPE_REQUEST_FRAG;

  pkt.bytes.comm.content.deliver_fragment_to.file_id  = file_id;
  pkt.bytes.comm.content.deliver_fragment_to.frag_id  = fragment_id;
  pkt.bytes.comm.content.deliver_fragment_to.to       = *deliver_to;
  pkt.size = sizeof(pkt.bytes.comm) + sizeof(pkt.size);

  printf("SENDING FRAG DELIVER REQUEST.\n");
  server_send_to_socket(sv, &pkt, fd);

  if ( ! server_wait_ok( sv, fd ) ) {
    printf("FRAGMENT REFUSED.\n");
    return -2;
  }

  server_close_socket( sv, fd );
  return 1;
}


int xprocedure_send_use_local( Server *sv, int fragment_id, Address *deliver_to ) 
{
  xPacket p = xpacket_new(sv, TYPE_DECLARE_USE_LOCAL);

  p.bytes.comm.content.declare_fragment_use_local.frag_id = fragment_id;
  p.size = sizeof(p.bytes.comm);

  int c = server_dial(sv, deliver_to);
  if (c <= 0)
    return -1 ;

  xPacket presentation = xpacket_presentation(sv);

  server_send_to_socket(sv, &presentation, c);

  if (!server_wait_ok(sv, c))
  {
    printf("PRESENTATION REFUSED.\n");
    return -2;
  }

  server_send_to_socket(sv, &p, c);

  if (!server_wait_ok(sv, c))
  {
    printf("FRAGMENT REFUSED.\n");
    return -3;
  }

  server_close_socket( sv, c );

  return 1;
}



/**
 * 
 * 
 */
int xprocedure_send_fragment( Server *sv, xFileServer *fs, int file_id, int fragment_id, Address *deliver_to ) 
{

  printf("CONNECTING TO :%d\n", deliver_to->port);

  int fd = server_dial( sv, deliver_to );

  if (fd <= 0)
  {
    return 0;
  }

  xFileContainer *fc = xfileserver_find_file(fs, file_id);
  if (fc == NULL)
  {
    return -1;
  }

  xFileFragment *fragments = fc->fragments;
  xFileFragment *fragment = NULL;

  for (int i = 0; i < REDUNDANCY; i++)
  {
    if (fragments[i].fragment_id == fragment_id)
    {
      fragment = fragments + i;
      break;
    }
  }

  if (fragment == NULL)
  {
    return -2;
  }

  xPacket presentation = xpacket_presentation(sv);
  server_send_to_socket(sv, &presentation, fd);
  if ( ! server_wait_ok( sv, fd ) ) {
    printf("FRAGMENT REFUSED.\n");
    return -3;
  }
  
  xPacket p = {0};
  p.bytes.comm.sender_id  = sv->me.node_id;
  p.bytes.comm.type       = TYPE_DECLARE_FRAG;
  p.bytes.comm.content.declare_fragment_transport.file_id   = file_id;
  p.bytes.comm.content.declare_fragment_transport.frag_id   = fragment_id;
  p.bytes.comm.content.declare_fragment_transport.frag_size = fragment->fragment_size;
  p.bytes.comm.content.declare_fragment_transport.file_size = fc->size;
  p.size = sizeof(p.bytes.comm) + sizeof(p.size);

  printf("SENDING FRAG DECLARATION\n");
  server_send_to_socket(sv, &p, fd);

  if ( ! server_wait_ok( sv, fd ) ) {
    printf("FRAGMENT REFUSED.\n");
    return -4;
  }

  int r = server_send_large_buffer_to( sv, fd, fragment->fragment_size, fragment->fragment_bytes);

  if ( ! server_wait_ok( sv, fd ) ) {
    printf("FRAGMENT RAW REFUSED.\n");
    return -5;
  }

  server_close_socket( sv, fd );

  return 1;
}


/**
 * 
 *  ------------------------------------------------------------
 */
int xprocedure_peer_died_notify( Server *sv )
{
  printf("[PROC] : NOTIFY PEER DEATH. \n");

  xPacket p = xpacket_peer_dead( sv, sv->peer_b.node_id );

  xprocedure_peer_died_forward(sv, &p);

  sv->net_size--;

}

int xprocedure_peer_died_forward( Server *sv, xPacket *p )
{

  p->bytes.comm.sender_id = sv->me.node_id;

  printf("[PROC] : FORWARD PEER DEATH. \n");

  int i = server_send_to_peer_f(sv, p);

  printf("[PROC] : %d\n", i); 
}


int xprocedure_index_died_notify( Server *sv )
{
   
}


int xprocedure_save_file_to_index( Server *sv, xFileServer *fs, xFileNetworkIndex *fnetidx, xReportFileKnowledge *r, int c)
{
  printf("INIT PROC \n" );
  int fragments_redundancy = r->frag_count * REDUNDANCY;

  // xfilenetindex_debug(fnetidx);
  // xfileserver_debug(fs);

  xFileInNetwork *fni = xfilenetindex_find_file(fnetidx, r->file_id);
  xFileContainer *fc = xfileserver_find_file(fs, r->file_id);
  if (fni == NULL) // if file is not created yet
  {
    printf("SETTING INDEX\n");
    fni = xfilenetindex_new_file(r->file_id, r->frag_count * REDUNDANCY);
    xfilenetindex_add_file(fnetidx, fni);
  }
  if (fc == NULL) // if file is not created yet
  {
    printf("SETTING FILE CONTAINER\n");
    fc = xfileserver_add_file(fs, r->file_name, r->file_id, r->file_size, r->frag_count);
  }

  int frags[2] = {0, 0};

  for (int i = 0; i < 2; i++)
  {
    printf("  Fragment %d:\n", i);
    printf("    fragment: %u\n", r->fragments[i].fragment);
    printf("    size: %llu\n", (unsigned long long)r->fragments[i].size);
    printf("    node_id: %llu\n", (unsigned long long)r->fragments[i].node_id);
    frags[i] = r->fragments[i].fragment;
  }

  int diff = frags[1] - frags[0];

  printf("DIFF BETWEEN FRAGS: %d\n", diff);
 
  // if ony onde fragment, than there will be no redundancy per node, only in the next neighbour
  if ( r->frag_count == 1 ) {
    printf("HALDLING 1 FRAG FILE.\n");
    if ( fni->fragments[0].fragment != 0 )
    {
      if ( fni->fragments[0].node_id < r->fragments[0].node_id )
      {
        fni->fragments[1] = r->fragments[0];
      }
      else {
        fni->fragments[1] = fni->fragments[0];
        fni->fragments[0] = r->fragments[0];
      }
    }
    else {
      fni->fragments[0] = r->fragments[0];
    }
  }
  else if ( diff > 1 ) // iff diff between frag ids is negative, thes isthe borders
  {
    fni->fragments[0] = r->fragments[0];
    fni->fragments[r->frag_count * REDUNDANCY - 1] = r->fragments[1];
  }
  else
  {
    if ( r->frag_count == 2 && fni->fragments[0].fragment == 0 ) {
      fni->fragments[0] = r->fragments[0];
      fni->fragments[r->frag_count * REDUNDANCY - 1] = r->fragments[1];
    }
    else {
      size_t place = (frags[1] - 1) * REDUNDANCY;

      printf("Writing to fragments[%zu] and fragments[%zu]\n",
             place, place - 1);

      fni->fragments[place - 1] = r->fragments[0];
      fni->fragments[place] = r->fragments[1];
    }
  }

}