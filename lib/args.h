#ifndef ARGS_H
#define ARGS_H

typedef struct __Args {
    int id;
    char ip[64];
    int peer_id;
    char peer_ip[64];
    int netsize;
} Args;


int parse_args(int argc, char **argv, Args *args);
void debug_args_inline(const Args *args);

#endif

