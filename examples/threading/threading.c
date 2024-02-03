#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    // wait
    int rc = usleep(thread_func_args->wait_2_obtain_ms * 1000);
    if (rc != 0) {
        ERROR_LOG("usleep() failure before obtaining the mutex");
        return thread_param;
    }

    // acquire mutex
    rc = pthread_mutex_lock(thread_func_args->mu);
    if (rc != 0) {
        ERROR_LOG("Couldn't lock the mutex");
        return thread_param;
    }

    // wait before release
    rc = usleep(thread_func_args->wait_2_release_ms * 1000);
    if (rc != 0) {
        ERROR_LOG("usleep() failure before releasing the mutex");
        pthread_mutex_unlock(thread_func_args->mu);
        return thread_param;
    }

    // release the mutex
    rc = pthread_mutex_unlock(thread_func_args->mu);
    if (rc != 0) {
        ERROR_LOG("Failed to unlock the mutex");
        return thread_param;
    }

    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    // allocating thread data
    struct thread_data* arg = malloc(sizeof(struct thread_data));
    if (arg == NULL) {
        ERROR_LOG("Failed to allocate thread data");
        return false;
    }

    // filling in thread data
    arg->wait_2_obtain_ms = wait_to_obtain_ms;
    arg->wait_2_release_ms = wait_to_release_ms;
    arg->mu = mutex;
    arg->thread_complete_success = false;

    int rc = pthread_create(thread, NULL, &threadfunc, arg);
    if (rc != 0) {
        ERROR_LOG("Failed to start the thread");
        return false;
    }

    return true;
}

