#ifndef	_RECV_SEND_H
#define	_RECV_SEND_H 1

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

typedef struct com_data_t {
    int conn_fd;
    FILE* sink;
    pthread_mutex_t* mutex;
    volatile bool execution_finished;
    volatile sig_atomic_t* interrupt_flag;
    ssize_t result;
} com_data_t;

ssize_t exchange_cycle(com_data_t* com_data);

#endif // _RECV_SEND_H