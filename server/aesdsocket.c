#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <arpa/inet.h>

#define SELF_PORT "9000"
#define N_PEDNING_CONNECTIONS 10
#define MAXDATASIZE 100 // max number of bytes we can get at once 

// if output_fd is zero, opens output stream and returns associated file descriptor
// negative return value denotes an error
int write_to_file(int output_fd, char buf[10], int buf_len) {
    return 0;
}

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char** argv) {
    int sock_fd, rc;

    //----------starting a socket for listening to incoming connections--------->//
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* addr_info = NULL; // address info to fill
    rc = getaddrinfo(NULL, SELF_PORT, &hints, &addr_info);
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
    freeaddrinfo(addr_info);

    rc = listen(sock_fd, N_PEDNING_CONNECTIONS);
    if (rc < 0) {
        perror("Error by calling to listen");
        exit(-1);
    }
    //<---------starting a socket for listening to incoming connections----------//
    
    while(1) { // accepting connections in a loop
        struct sockaddr_storage remote_info;
        socklen_t remote_info_size = sizeof remote_info;
        int conn_fd = accept(sock_fd, (struct sockaddr*)&remote_info, &remote_info_size);
        if (conn_fd < 0) {// invalid socket descriptor
            perror("Couldn't accept a connection");
            continue;
        }

        // sending a notifciation on connection to syslog
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(remote_info.ss_family, get_in_addr((struct sockaddr *)&remote_info), client_ip, sizeof client_ip);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        FILE* sink = fopen("/var/tmp/aesdsocketdata", "w+");
        if (!sink) {
            perror("Couldn't open /var/tmp/aesdsocketdata");
            continue;
        }

        char buffer[256] = {0};
        while((rc = recv(conn_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[rc] = '\0';
            fwrite(buffer, 1, rc, sink);
        }

        if (rc == 0) { // remote has closed the connection
            syslog(LOG_INFO, "Closed connection from %s", client_ip);
        }
        else { // an error occured
            perror("Error while receiving data");
        }
        close(conn_fd);
        
        fclose(sink);
    }

    exit(0);
}