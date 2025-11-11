#include "args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAG_ID       "-id"
#define FLAG_IP       "-ip"
#define FLAG_PEER_ID  "-peer-id"
#define FLAG_PEER_IP  "-peer-ip"
#define FLAG_NETSIZE  "-network-size"

void debug_args_inline(const Args *args) {
    printf("[Args] id=%d ip=%s peer_id=%d peer_ip=%s netsize=%d\n",
           args->id, args->ip, args->peer_id, args->peer_ip, args->netsize);
}

int parse_args(int argc, char **argv, Args *args) {

    args->id            = 0;
    args->peer_id       = 0;
    args->netsize       = 0;
    args->peer_ip[0]    = '\0';
    args->ip[0]         = '\0';

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], FLAG_ID) == 0 && i + 1 < argc) {
            args->id = atoi(argv[++i]);
            continue;
        }

        if (strcmp(argv[i], FLAG_IP) == 0 && i + 1 < argc) {
            strncpy(args->ip, argv[++i], sizeof(args->ip) - 1);
            args->ip[sizeof(args->ip) - 1] = '\0';
            continue;
        }

        if (strcmp(argv[i], FLAG_PEER_ID) == 0 && i + 1 < argc) {
            args->peer_id = atoi(argv[++i]);
            continue;
        }

        if (strcmp(argv[i], FLAG_PEER_IP) == 0 && i + 1 < argc) {
            strncpy(args->peer_ip, argv[++i], sizeof(args->peer_ip) - 1);
            args->peer_ip[sizeof(args->peer_ip) - 1] = '\0';
            continue;
        }

        if (strcmp(argv[i], FLAG_NETSIZE) == 0 && i + 1 < argc) {
            args->netsize = atoi(argv[++i]);
            continue;
        }

        fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
        return 0;
    }

    if (args->id <= 0 || args->peer_id <= 0 ) {
        fprintf(stderr, "Usage: %s %s N %s ADDR %s N %s ADDR %s N\n",
                argv[0],
                FLAG_ID, FLAG_IP, FLAG_PEER_ID, FLAG_PEER_IP, FLAG_NETSIZE);
        fprintf(stderr, "Invalid id:  node-id=%d peer-id=%d. \n", args->id, args->peer_id);
        return 0;
    }

    if ( args->netsize <= 0 || strlen(args->ip) == 0 || strlen(args->peer_ip) == 0) {
        fprintf(stderr, "Usage: %s %s N %s ADDR %s N %s ADDR %s N\n",
                argv[0],
                FLAG_ID, FLAG_IP, FLAG_PEER_ID, FLAG_PEER_IP, FLAG_NETSIZE);
        fprintf(stderr, "Invalid ip. \n");
        return 0;
    }

    return 1;
}

