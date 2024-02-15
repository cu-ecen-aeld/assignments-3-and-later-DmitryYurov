#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <arpa/inet.h>

#define SELF_PORT "9000"
#define N_PEDNING_CONNECTIONS 10
#define STORAGE_FILE "/var/tmp/aesdsocketdata"

bool signal_caught = false;
static void signal_handler(int signal) {
    signal_caught = true;
}

int install_sighandlers() {
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));

    new_action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &new_action, NULL) != 0) return -1;
    if (sigaction(SIGINT, &new_action, NULL) != 0) return -1;

    return 0;
}

ssize_t exchange_cycle(int conn_fd, FILE* sink) {
    char input_buffer[256] = {0};
    char output_buffer[256] = {0};
    ssize_t bytes_recv, bytes_read;

    while((bytes_recv = recv(conn_fd, input_buffer, sizeof(input_buffer) - 1, 0)) > 0 && !signal_caught) {
        input_buffer[bytes_recv] = '\0';
        char* pos = strchr(input_buffer, '\n');
        if (!pos) { // packet not finished, continue receive cycle
            fwrite(input_buffer, 1, bytes_recv, sink);
        } else {
            fwrite(input_buffer, 1, pos - input_buffer + 1, sink); // writing till the newline
            rewind(sink);
            while ((bytes_read = fread(output_buffer, 1, sizeof(output_buffer), sink)) > 0)
                send(conn_fd, output_buffer, bytes_read, 0);

            ssize_t bytes_left = bytes_recv - (pos - input_buffer + 1);
            printf("Bytes left = %zd\n", bytes_left);
            if (bytes_left > 0) fwrite(pos + 1, 1, bytes_left, sink);
        }
    }

    return bytes_recv;
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
        close(sock_fd);
        exit(-1);
    }
    freeaddrinfo(addr_info); // don't need addr_info anymore

    rc = listen(sock_fd, N_PEDNING_CONNECTIONS);
    if (rc < 0) {
        perror("Error by calling to listen");
        close(sock_fd);
        exit(-1);
    }
    //<---------starting a socket for listening to incoming connections----------//
    
    //----------opening a file to store the socket messages--------------------->//
    FILE* sink = fopen(STORAGE_FILE, "w+");
    if (!sink) {
        perror("Couldn't open /var/tmp/aesdsocketdata");
        close(sock_fd);
        exit(-1);
    }
    //<---------opening a file to store the socket messages----------------------//

    if (install_sighandlers() != 0) {
        perror("Couldn't install signal handlers");
        close(sock_fd);
        exit(-1);
    }

    //----------data exchange loop---------------------------------------------->//
    while(!signal_caught) { // accepting connections in a loop
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

        if (exchange_cycle(conn_fd, sink) >= 0) { // remote has closed the connection
            syslog(LOG_INFO, "Closed connection from %s", client_ip);
        }
        else { // an error occured
            perror("Error during data exchange");
        }

        close(conn_fd);
    }
    //<---------data exchange loop-----------------------------------------------//

    if (signal_caught) syslog(LOG_INFO, "Caught signal, exiting");

    close(sock_fd);
    fclose(sink);

    // deleting packet storage
    if (remove(STORAGE_FILE) != 0)
        perror("Couldn't delete storage file");

    exit(0);
}