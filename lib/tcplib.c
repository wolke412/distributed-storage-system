
// tcplib.c
#include "tcplib.h"
#include "nettypes.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>



tcp_socket tcp_listen(int port) {

    tcp_socket sockfd;
    struct sockaddr_in addr;
    int opt = 1;
    
    /*
     * domain   = ipv4, 
     * type     = SOCK_STREAM is TCP
     */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    // Allow quick reuse of port after restart
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // non-blocking socket
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 8) < 0) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}


int tcp_try_accept(int server_fd)
{
    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;

    int r = poll(&pfd, 1, 0);  // timeout = 0 ms -> non-blocking check
    if (r < 0) {
        perror("poll");
        return -1;
    }

    if (r == 0) {
        // No client waiting
        return 0;
    }

    int client = accept(server_fd, NULL, NULL);
    if (client < 0) {
        perror("accept");
        return -1;
    }

    return client;
}



tcp_socket tcp_accept(tcp_socket server_sock) {

    struct sockaddr_in client_addr;

    socklen_t len = sizeof(client_addr);

    tcp_socket client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &len );

    if (client_sock < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NO_CONNECTION_WAITING;
        } else {
            perror("accept");
        }

        return -1;
    }

    // printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    return client_sock;
}

int tcp_peek(tcp_socket client_sock, void *buffer, size_t len) {
    return recv(client_sock, buffer, len, MSG_PEEK);
}

int tcp_peek_u(tcp_socket client_sock, void *buffer, size_t len ) {
    return recv(client_sock, buffer, len, MSG_DONTWAIT | MSG_PEEK);
}


size_t tcp_has_data(tcp_socket client_sock) {
    int i = 0;
    return recv(client_sock, &i, 0, MSG_PEEK);
}

int FD_tcp_has_data(tcp_socket sock) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval tv = {0, 0}; // no wait, immediate return
    int res = select(sock + 1, &readfds, NULL, NULL, &tv);

    if (res > 0 && FD_ISSET(sock, &readfds)) {
        return 1; // data available
    }
    return 0; // no data
}


int tcp_recv(tcp_socket client_sock, void *buffer, size_t len ) {
    return recv(client_sock, buffer, len, 0);
}

int tcp_recv_u(tcp_socket client_sock, void *buffer, size_t len ) {
    return recv(client_sock, buffer, len, MSG_DONTWAIT);
}

int tcp_recv_flags(tcp_socket client_sock, void *buffer, size_t len, int flags) {
    return recv(client_sock, buffer, len, flags);
}

int tcp_send(tcp_socket client_sock, const void *buffer, size_t len) {
    // printf("SENDING BUF *%p SIZE=%d \n", buffer, len);

    const char *buf = (const char *)buffer;
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t n = send(client_sock, buf + total_sent, len - total_sent, 0);

        // printf("__ sent %d bytes\n", total_sent+n);
        if (n > 0) {
            total_sent += n;
            continue;
        }

        if (n <= 0) {
            // Socket would block → wait a bit and retry
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // sleep a tiny bit to avoid spinning the CPU
                usleep(1000); 
                continue;
            }

            // Actual error
            perror("send");
            return total_sent;  // bytes sent before failure
        }
    }

    return total_sent;
}


int tcp_send_u(tcp_socket client_sock, const void *buffer, size_t len) {
    return send(client_sock, buffer, len, MSG_NOSIGNAL | MSG_DONTWAIT);
}

void tcp_close(tcp_socket sock) {
    close(sock);
}


// ------------------------------------------------------------ 
int tcp_open(const Address *address) {
    if (!address) {
        fprintf(stderr, "tcp_connect: invalid address\n");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(address->port);

    memcpy(&addr.sin_addr.s_addr, address->ip.octet, 4);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
