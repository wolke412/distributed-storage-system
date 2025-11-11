#include "nettypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ipv4_from_string( IPv4 *ip, const char *str ) {

    if (!str) return 0;
    
    // if string is empty initialize to localhost also
    if ( str[0] == '\0' || strcmp(str, "localhost") == 0) {
        ip->octet[0] = 127;
        ip->octet[1] = 0;
        ip->octet[2] = 0;
        ip->octet[3] = 1;
        return 1;
    }

    unsigned int a, b, c, d;

    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        ip->octet[0] = (uint8_t)a;
        ip->octet[1] = (uint8_t)b;
        ip->octet[2] = (uint8_t)c;
        ip->octet[3] = (uint8_t)d;
    }

    return 1; // success
}

void ipv4_to_string(const IPv4 *ip, char *out, size_t size) {
    if (!ip || !out || size < 16) return;
    snprintf(out, size, "%u.%u.%u.%u",
             ip->octet[0],
             ip->octet[1],
             ip->octet[2],
             ip->octet[3]);
}

int address_from_string(Address *addr, const char *str) {

    if (!addr || !str) 
    {
        return 0;
    }

    addr->port = 0;


    char buf[32];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // split IP and port 
    char *colon = strchr(buf, ':');
    if (colon)  
    { 
        *colon = '\0'; // terminate IP part
        addr->port = (uint16_t)atoi(colon + 1);
    }

    return ipv4_from_string( &addr->ip, buf );
}

void address_to_string(const Address *addr, char *out, size_t size) {
    if (!addr || !out || size < 22) return;
    snprintf(out, size, "%u.%u.%u.%u:%u",
             addr->ip.octet[0],
             addr->ip.octet[1],
             addr->ip.octet[2],
             addr->ip.octet[3],
             addr->port);
}

void debug_address(const Address *addr) {
    if (!addr) {
        printf("[Address: (null)]\n");
        return;
    }

    printf("[Address %u.%u.%u.%u:%u]\n",
           addr->ip.octet[0],
           addr->ip.octet[1],
           addr->ip.octet[2],
           addr->ip.octet[3],
           addr->port);
}

