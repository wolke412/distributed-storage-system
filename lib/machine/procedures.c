#include "statemachine.h"

int xprocedure_send_request_fragment( Server *sv, Address *to , int file_id, int fragment_id, Address *deliver_to )
{
  int fd = server_dial(sv, to);
  if (fd <= 0)
  {
    return fd;
  }

  xPacket pkt = {0};

  pkt.bytes.comm.sender_id = sv->me.node_id;




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
