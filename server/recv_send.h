#ifndef	_RECV_SEND_H
#define	_RECV_SEND_H 1

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

ssize_t exchange_cycle(int conn_fd, FILE* sink, volatile sig_atomic_t* interrupt_flag);

#endif // _RECV_SEND_H