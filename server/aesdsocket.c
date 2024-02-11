#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char* const port = "9000";


int main(int argc, char** argv) {
    int sock_fd, rc;
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    struct addrinfo* addr_info = NULL; // address info to fill

    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    rc = getaddrinfo(NULL, port, &hints, &addr_info);
    if (rc != 0) {
        fprintf(stderr, "Failure by composing address info: %s\n", gai_strerror(rc));
        freeaddrinfo(addr_info);
        exit(-1);
    }

    sock_fd = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
    if (sock_fd < 0) {
        perror("Couldn't create a socket");
        freeaddrinfo(addr_info);
        exit(-1);
    }

    rc = bind(sock_fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (rc < 0) {
        perror("Couldn't bind the socket");
        freeaddrinfo(addr_info);
        exit(-1);
    }

    rc = connect(sock_fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (rc < 0) {
        perror("Couldn't connect");
        freeaddrinfo(addr_info);
        exit(-1);
    }

    freeaddrinfo(addr_info);
    exit(0);
}