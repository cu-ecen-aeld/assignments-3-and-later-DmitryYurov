#include "conn_threads.h"

#include <stdatomic.h>
#include <stdlib.h>

static void* thread_func(void* arg) {
    if (!arg) return NULL;

    com_data_t* com_data = (com_data_t*)arg;
    com_data->result = exchange_cycle(com_data);
    atomic_exchange(&com_data->execution_finished, true);

    return NULL;
}

com_data_t* create_com_data() {
    com_data_t* result = malloc(sizeof(com_data_t));
    if (!result) return result;

    result->conn_fd = 0;
    result->execution_finished = false;
    result->interrupt_flag = NULL;
    result->mutex = NULL;
    result->result = 0;

    return result;
}

void init_thread_list(head_t* head) {
    SLIST_INIT(head);
}

int enqueue_thread(head_t* head, com_data_t* com_data) {
    if (!com_data) return -1;
    thread_node_t* node = malloc(sizeof(thread_node_t));
    if (!node) return -1;
    node->com_data = com_data;

    int rc = pthread_create(&node->id, NULL, thread_func, com_data);
    if (rc != 0) {
        if (node->com_data->conn_fd >= 0) close(node->com_data->conn_fd);
        free(node->com_data);
        free(node);
        return rc;
    }

    SLIST_INSERT_HEAD(head, node, nodes);
    return 0;
}

void remove_ready(head_t* head, bool wait) {
    thread_node_t* node = NULL;
    thread_node_t* next = NULL;
    SLIST_FOREACH_SAFE(node, head, nodes, next) {
        if (node->com_data->execution_finished || wait) {
            pthread_join(node->id, NULL);
            SLIST_REMOVE(head, node, thread_node_t, nodes);
            if (node->com_data->conn_fd >= 0) close(node->com_data->conn_fd);
            free(node->com_data);
            free(node);
            node = NULL;
        }
    }
}
