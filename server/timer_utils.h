#ifndef _TIMER_UTILS_H
#define _TIMER_UTILS_H 1

#include <pthread.h>
#include <stdio.h>

typedef struct timer_event_t {
    FILE* sink;
    pthread_mutex_t* mutex;
} timer_event_t;

int setup_timer(timer_event_t event_data);
int disarm_timer();

#endif // _TIMER_UTILS_H