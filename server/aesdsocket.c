#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "conn_threads.h"

// uncomment the following line to enable redirection to the aesdchar device
#define USE_AESD_CHAR_DEVICE 1

#define SELF_PORT "9000"
#define N_PEDNING_CONNECTIONS 10

volatile sig_atomic_t signal_caught = 0;
static void signal_handler(int signal) {
    signal_caught += 1;
}

int install_sighandlers() {
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));

    new_action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &new_action, NULL) != 0) return -1;
    if (sigaction(SIGINT, &new_action, NULL) != 0) return -1;

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
    int sock_fd;
    int rc = 0;
    int exit_code = 0;

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
        exit_code = -1;
        goto exit_label;
    }

    sock_fd = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
    if (sock_fd < 0) {
        perror("Couldn't create a socket");
        freeaddrinfo(addr_info);
        exit_code = -1;
        goto exit_label;
    }

    rc = bind(sock_fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (rc < 0) {
        perror("Couldn't bind the socket");
        freeaddrinfo(addr_info); // calling freeaddrinfo before perror can cause a change in errno
        exit_code = -1;
        goto close_socket_label;
    }
    freeaddrinfo(addr_info); // don't need addr_info anymore
    //<---------starting a socket for listening to incoming connections----------//

    //----------creating a daemon----------------------------------------------->//
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        rc = fork();
        if (rc < 0) {
            perror("Couldn't fork the process");
            exit_code = -1;
            goto close_socket_label;
        } else if (rc > 0) { // parent process
            goto exit_label;
        }

        // child process
		if(setsid() < 0) { // moving to a different session (disconnect from the terminal)
            syslog(LOG_ERR, "Error by setting session id: %d (%s)", errno, strerror(errno));
			exit_code = -1;
            goto close_socket_label;
		}
        chdir("/");
        close(STDIN_FILENO);
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);
    }
    //<---------creating a daemon------------------------------------------------//

    rc = listen(sock_fd, N_PEDNING_CONNECTIONS);
    if (rc < 0) {
        syslog(LOG_ERR, "Error by calling to listen: %d (%s)", errno, strerror(errno));
        exit_code = -1;
        goto close_socket_label;
    }

    if (install_sighandlers() != 0) {
        syslog(LOG_ERR, "Couldn't install signal handlers: %d (%s)", errno, strerror(errno));
        exit_code = -1;
        goto close_socket_label;
    }

    //----------initialize threading-related primitives------------------------->//
    head_t head;
    init_thread_list(&head);
    pthread_mutex_t th_mutex;
    if (pthread_mutex_init(&th_mutex, NULL) != 0) {
        syslog(LOG_ERR, "Couldn't initialize a pthread mutex: %d (%s)", errno, strerror(errno));
        exit_code = -1;
        goto close_socket_label;
    }
    //<---------initialize threading-related primitives--------------------------//


    //----------data exchange loop---------------------------------------------->//
    while(!signal_caught) { // accepting connections in a loop
        struct sockaddr_storage remote_info;
        socklen_t remote_info_size = sizeof remote_info;
        int conn_fd = accept(sock_fd, (struct sockaddr*)&remote_info, &remote_info_size);
        if (conn_fd < 0) {// invalid socket descriptor
            syslog(LOG_ERR, "Couldn't accept a connection: %d (%s)", errno, strerror(errno));
            continue;
        }

        // sending a notifciation on connection to syslog
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(remote_info.ss_family, get_in_addr((struct sockaddr *)&remote_info), client_ip, sizeof client_ip);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        com_data_t* com_data = create_com_data();
        if (!com_data) {
            syslog(LOG_ERR, "Failed to create communication data: %d (%s)", errno, strerror(errno));
            close(conn_fd);
            continue;
        }

        com_data->conn_fd = conn_fd;
        com_data->interrupt_flag = &signal_caught;
        com_data->mutex = &th_mutex;
        if (enqueue_thread(&head, com_data) != 0) {
            syslog(LOG_ERR, "Failed to enqueue thread");
        }

        remove_ready(&head, false);
    }
    //<---------data exchange loop-----------------------------------------------//

    if (signal_caught) syslog(LOG_INFO, "Caught signal, exiting");

    remove_ready(&head, true);
    pthread_mutex_destroy(&th_mutex);
    
close_socket_label:
    close(sock_fd);

exit_label:
    exit(exit_code);
}
