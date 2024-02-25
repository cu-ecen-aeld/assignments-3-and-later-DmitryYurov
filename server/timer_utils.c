#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "timer_utils.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

timer_t timer_id;
timer_event_t event_data_intern;

static void on_timer(__sigval_t timer_data) {
    const timer_event_t* event_data = (const timer_event_t*)timer_data.sival_ptr;

    struct timespec ts;
    int rc = clock_gettime(CLOCK_REALTIME, &ts);
    if (rc != 0) {
        syslog(LOG_ERR, "Error while acessing clock_gettime: %d (%s)", errno, strerror(errno));
        return;
    }

    struct tm time_breakdown;
    if (!localtime_r(&ts.tv_sec, &time_breakdown)) {
        syslog(LOG_ERR, "Error while acessing localtime_r: %d (%s)", errno, strerror(errno));
        return;
    }

    char buf[80] = {0};
    int n_bytes_written = strftime(buf, 80, "timestamp:%Y/%b/%a/%H:%M:%S\n", &time_breakdown);
    if (!n_bytes_written) {
        syslog(LOG_ERR, "Creating formatted time string failed: %d (%s)", errno, strerror(errno));
        return;
    }

    if ((rc = pthread_mutex_lock(event_data->mutex)) != 0) {
        syslog(LOG_ERR, "Locking file mutex failed: %d (%s)", errno, strerror(errno));
        return;
    }
    fwrite(buf, 1, n_bytes_written, event_data->sink);
    pthread_mutex_unlock(event_data->mutex);
}

int setup_timer(timer_event_t event_data) {
    event_data_intern = event_data;
    int rc = 0;

    struct sigevent event = { 0 };
    event.sigev_notify = SIGEV_THREAD;
    event.sigev_notify_function = &on_timer;
    event.sigev_value.sival_ptr = &event_data_intern;

    rc = timer_create(CLOCK_MONOTONIC, &event, &timer_id);
    if (rc != 0) {
        syslog(LOG_ERR, "Error by setting up a timer: %d (%s)", errno, strerror(errno));
        return rc;
    }

    struct itimerspec its = {
        .it_interval.tv_sec = 10,
        .it_interval.tv_nsec = 0,
        .it_value.tv_sec = 10,
        .it_value.tv_nsec = 0
    };

    rc = timer_settime(timer_id, 0, &its, NULL);
    if (rc != 0) syslog(LOG_ERR, "Error by setting up a timer: %d (%s)", errno, strerror(errno));

    return rc;
}

int disarm_timer() {
    int rc = timer_delete(timer_id);
    if (rc != 0) syslog(LOG_ERR, "Failed to delete timer: %d (%s)", errno, strerror(errno));

    return rc;
}
