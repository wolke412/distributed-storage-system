#ifndef NET_TYPES_H
#define NET_TYPES_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint8_t octet[4];
} IPv4;

typedef struct {
    IPv4 ip;
    uint16_t port;
} Address;

// ------------------------------------------------------------

int ipv4_from_string( IPv4 *ip, const char *str );
void ipv4_to_string(const IPv4 *ip, char *out, size_t size);
int address_from_string(Address *addr, const char *str);
void address_to_string(const Address *addr, char *out, size_t size);

//
void debug_address(const Address *addr);

#endif // NET_TYPES_H
