
#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#define FILE_SERVER_MAX_FILES     ( 100 )

#define __FILE_FRAGMENT_ID_TYPE__ uint8_t // no need for more than 256 fragments for a single file


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>




typedef struct xFileFragment {
  __FILE_FRAGMENT_ID_TYPE__ fragment_id; // 0, 1, 2, 3... ,
  char *fragment_bytes;
  uint64_t fragment_size;
} xFileFragment;

typedef struct xFileContainer {
  char file_name[256];
  uint16_t file_id;
  uint64_t size; 
  uint8_t fragment_count_total;

  xFileFragment fragments[2];
  uint8_t *fragment_bytes;
} xFileContainer;

typedef struct xFileServer{
  uint16_t file_count;
  xFileContainer *files;
} xFileServer;
// ------------------------------------------------------------ 



int xfileserver_init(xFileServer *fs);
  
xFileContainer *xfileserver_add_file(
    xFileServer *index,
    const char *file_name,
    uint16_t file_id,
    uint64_t total_size,
    uint8_t fragment_count_total
);


int xfileserver_add_fragment(
    xFileContainer *file,
    uint8_t fragment_index,
    const void *data,
    uint64_t size
);

void xfileserver_free_file(xFileContainer *file);
void xfileserver_free_fs(xFileServer *fs);


// ------------------------------------------------------------
void xfileserver_debug(const xFileServer *fs);


#endif // FILE_SERVER_H