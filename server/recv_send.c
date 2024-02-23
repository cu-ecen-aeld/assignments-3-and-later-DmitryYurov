#include "recv_send.h"

#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

typedef struct InputData {
    void* data;
    int size;
    int left;
} InputData;

static int init_input_data(InputData* data) {
    data->data = calloc(256, 1);
    data->size = 256;
    data->left = 256;

    return data->data != NULL ? 0 : 1;
}

static int recalc(InputData* data, int bytes_written) {
    data->left -= bytes_written;
    if (data->left < data->size / 2) {
        data->left += data->size;
        data->size = 2 * data->size;
        data->data = realloc(data->data, data->size);
    }

    return data->data != NULL ? 0 : 1;
}

static void* eodata(InputData* data) {
    if (data->left >= data->size) return NULL;
    return data->data + (data->size - data->left);
}

static void move_last_n(InputData* data, int n) {
    if (n > 0) {
        memmove(data->data, data->data + (data->size - n), n);
        data->left = data->size - n;
    } else {
        data->left = data->size;
    }
}

static void free_input_data(InputData* data) {
    free(data->data);
}


ssize_t exchange_cycle(int conn_fd, FILE* sink, volatile sig_atomic_t* interrupt_flag) {
    char output_buffer[256] = {0};
    ssize_t bytes_recv = 0;
    ssize_t bytes_read = 0;

    InputData input_buffer;
    int rc = init_input_data(&input_buffer);
    if (rc) return rc;

    while(!interrupt_flag || !*interrupt_flag) {
        void* eobuf = eodata(&input_buffer);
        bytes_recv = recv(conn_fd, eobuf, input_buffer.left - 1, 0);
        if (bytes_recv <= 0) break;

        ((char*)eobuf)[bytes_recv] = '\0';
        char* pos = strchr((char*)eobuf, '\n');
        if ((rc = recalc(&input_buffer, bytes_recv)) != 0) {
            free_input_data(&input_buffer);
            return rc;
        }
        if (pos) {
            const int n_bytes = (void*)pos - input_buffer.data + 1;
            fwrite(input_buffer.data, 1, n_bytes, sink); // writing till the newline
            rewind(sink);
            while ((bytes_read = fread(output_buffer, 1, sizeof(output_buffer), sink)) > 0)
                send(conn_fd, output_buffer, bytes_read, 0);

            move_last_n(&input_buffer, bytes_recv - n_bytes);
        }
    }

    return bytes_recv;
}