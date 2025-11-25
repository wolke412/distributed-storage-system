#ifndef TCPLIB_H
#define TCPLIB_H

#include "nettypes.h"
#include <stddef.h> // size_t

#define NO_CONNECTION_WAITING           (-2)

typedef int tcp_socket;

tcp_socket tcp_listen(int port);

tcp_socket tcp_accept(tcp_socket server_sock);

int tcp_peek(tcp_socket client_sock, void *buffer, size_t len);
int tcp_peek_u(tcp_socket client_sock, void *buffer, size_t len );

size_t tcp_has_data(tcp_socket client_sock);
int FD_tcp_has_data(tcp_socket sock);

int tcp_recv(tcp_socket client_sock, void *buffer, size_t len);
int tcp_recv_u(tcp_socket client_sock, void *buffer, size_t len); 
int tcp_recv_flags(tcp_socket client_sock, void *buffer, size_t len, int flags);

int tcp_send(tcp_socket client_sock, const void *buffer, size_t len);
int tcp_send_u(tcp_socket client_sock, const void *buffer, size_t len);

void tcp_close(tcp_socket sock);

int tcp_open(const Address *address);

#endif 

