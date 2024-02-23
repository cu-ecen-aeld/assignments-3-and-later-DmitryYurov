#ifndef	_CONN_THREADS_H
#define	_CONN_THREADS_H 1

#include "queue.h"
#include "recv_send.h"

typedef struct thread_node_t {
    pthread_t id;
    com_data_t* com_data;

    SLIST_ENTRY(thread_node_t) nodes;
} thread_node_t;
typedef SLIST_HEAD(head_s, thread_node_t) head_t;

com_data_t* create_com_data();

void init_thread_list(head_t* head);

int enqueue_thread(head_t* head, com_data_t* com_data);
void remove_ready(head_t* head, bool wait);

#endif // _CONN_THREADS_H
