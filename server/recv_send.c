#include "recv_send.h"

#include <sys/socket.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

typedef struct InputData {
    void* data;
    int size;
    int left;
} InputData;

static int init_input_data(InputData* data) {
    data->data = calloc(256, 1);
    data->size = 256;
    data->left = 256;

    return data->data != NULL ? 0 : -1;
}

static int recalc(InputData* data, int bytes_written) {
    data->left -= bytes_written;
    if (data->left < data->size / 2) {
        data->left += data->size;
        data->size = 2 * data->size;
        data->data = realloc(data->data, data->size);
    }

    return data->data != NULL ? 0 : -1;
}

static void* eodata(InputData* data) {
    if (data->left <= 0 || data->left > data->size) return NULL;
    return data->data + (data->size - data->left);
}

static void move_last_n(InputData* data, int n) {
    if (n > 0) {
        memmove(data->data, data->data + (data->size - data->left - n), n);
        data->left = data->size - n;
    } else {
        data->left = data->size;
    }
}

static void free_input_data(InputData* data) {
    free(data->data);
}


ssize_t exchange_cycle(com_data_t* com_data) {
    char output_buffer[256] = {0};
    int rc = 0;

    InputData input_buffer;
    if ((rc = init_input_data(&input_buffer))) goto finish;

    while(!*com_data->interrupt_flag) {
        void* eobuf = eodata(&input_buffer);
        ssize_t bytes_recv = recv(com_data->conn_fd, eobuf, input_buffer.left - 1, 0);
        if (bytes_recv <= 0) {
            rc = bytes_recv;
            goto finish;
        }

        ((char*)eobuf)[bytes_recv] = '\0';
        char* pos = strchr((char*)eobuf, '\n');
        if (pos) {
            const int n_bytes = (void*)pos - input_buffer.data + 1;

            // blocking mutex for io synchronization
            if ((rc = pthread_mutex_lock(com_data->mutex)) != 0) goto finish;
            int left_to_write = n_bytes;
            while (left_to_write > 0) {
                ssize_t write_rc = write(com_data->sink_fd, input_buffer.data, left_to_write);
                if (write_rc < 0) {
                    syslog(LOG_ERR, "Write operation failed: %d (%s)", errno, strerror(errno));
                    goto unlock_mutex;
                }
                left_to_write -= write_rc;
            }

            ssize_t bytes_read = read(com_data->sink_fd, output_buffer, sizeof(output_buffer));
            while (bytes_read != 0) {
                if (bytes_read < 0) {
                    syslog(LOG_ERR, "Read operation failed: %d (%s)", errno, strerror(errno));
                    goto unlock_mutex;
                }
                send(com_data->conn_fd, output_buffer, bytes_read, 0);
                bytes_read = read(com_data->sink_fd, output_buffer, sizeof(output_buffer));
            }   

unlock_mutex:
            pthread_mutex_unlock(com_data->mutex);

            if ((rc = recalc(&input_buffer, bytes_recv)) != 0) goto finish;
            move_last_n(&input_buffer, bytes_recv - n_bytes);
        }
        else if ((rc = recalc(&input_buffer, bytes_recv)) != 0) goto finish;
    }

finish:
    free_input_data(&input_buffer);
    return rc;
}