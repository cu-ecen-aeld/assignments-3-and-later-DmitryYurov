#include "recv_send.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "../aesd-char-driver/aesd_ioctl.h"

#define STORAGE_FILE "/dev/aesdchar"

static bool is_ioctl_cmd(const char* buf, size_t buf_len) {
    return buf_len > 22 && strncmp(buf, "AESDCHAR_IOCSEEKTO:", 19) == 0;
}

static struct aesd_seekto parse_ioctl_cmd(const char* buf, size_t buf_len) {
    struct aesd_seekto result;
    memset(&result, 0, sizeof(struct aesd_seekto));

    void* pos = memchr(buf, ',', buf_len);
    if (!pos) {
        syslog(LOG_ERR, "Failed to find command delimiter (,) in the command string");
        return result;
    }

    result.write_cmd = strtoul(buf + 19, NULL, 10);
    result.write_cmd_offset = strtoul(pos + 1, NULL, 10);
    syslog(LOG_INFO, "Read cmd and offset as %u, %u for ioctl command", result.write_cmd, result.write_cmd_offset);

    return result;
}

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
        char* pos = memchr((char*)eobuf, '\n', bytes_recv);
        if (pos) {
            const int n_bytes = (void*)pos - input_buffer.data + 1;
            if ((rc = pthread_mutex_lock(com_data->mutex)) != 0) goto finish; // blocking mutex for io synchronization

            //----------opening a file / device to store the socket messages--------------------->//
            int sink_fd = open(STORAGE_FILE, O_RDWR | O_CREAT, S_IRWXG | S_IRWXO | S_IRWXU);
            if (sink_fd < 0) {
                syslog(LOG_ERR, "Couldn't open %s: %d (%s)", STORAGE_FILE, errno, strerror(errno));
                goto unlock_mutex;
            }
            //<---------opening a file / device to store the socket messages----------------------//

            if (is_ioctl_cmd(input_buffer.data, n_bytes)) {
                struct aesd_seekto seek_data = parse_ioctl_cmd(input_buffer.data, n_bytes);
                if (ioctl(sink_fd, AESDCHAR_IOCSEEKTO, &seek_data) != 0) {
                    syslog(LOG_ERR, "Failed to execute ioctl request: %d (%s)", errno, strerror(errno));
                    goto close_sink;
                }
            } else {
                int left_to_write = n_bytes;
                while (left_to_write > 0) {
                    ssize_t write_rc = write(sink_fd, input_buffer.data, left_to_write);
                    if (write_rc < 0) {
                        syslog(LOG_ERR, "Write operation failed: %d (%s)", errno, strerror(errno));
                        goto close_sink;
                    }
                    left_to_write -= write_rc;
                }

                if (lseek(sink_fd, 0, SEEK_SET) < 0) {
                    syslog(LOG_ERR, "Seek operation failed: %d (%s)", errno, strerror(errno));
                    goto close_sink;
                }
            }

            ssize_t bytes_read = read(sink_fd, output_buffer, sizeof(output_buffer));
            while (bytes_read != 0) {
                if (bytes_read < 0) {
                    syslog(LOG_ERR, "Read operation failed: %d (%s)", errno, strerror(errno));
                    goto close_sink;
                }
                send(com_data->conn_fd, output_buffer, bytes_read, 0);
                bytes_read = read(sink_fd, output_buffer, sizeof(output_buffer));
            }

close_sink:
            if (close(sink_fd) != 0)
                syslog(LOG_ERR, "Couldn't close storage file %s: %d (%s)", STORAGE_FILE, errno, strerror(errno));

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
