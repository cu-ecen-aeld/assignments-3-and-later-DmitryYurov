#include "recv_send.h"

#include <sys/socket.h>

#include <string.h>

ssize_t exchange_cycle(int conn_fd, FILE* sink, volatile sig_atomic_t* interrupt_flag) {
    char input_buffer[256] = {0};
    char output_buffer[256] = {0};
    ssize_t bytes_recv = 0;
    ssize_t bytes_read = 0;

    while((!interrupt_flag || !*interrupt_flag) &&
          (bytes_recv = recv(conn_fd, input_buffer, sizeof(input_buffer) - 1, 0)) > 0
        ) {
        input_buffer[bytes_recv] = '\0';
        char* pos = strchr(input_buffer, '\n');
        if (!pos) { // packet not finished, continue receive cycle
            fwrite(input_buffer, 1, bytes_recv, sink);
        } else {
            fwrite(input_buffer, 1, pos - input_buffer + 1, sink); // writing till the newline
            rewind(sink);
            while ((bytes_read = fread(output_buffer, 1, sizeof(output_buffer), sink)) > 0)
                send(conn_fd, output_buffer, bytes_read, 0);

            ssize_t bytes_left = bytes_recv - (pos - input_buffer + 1);
            if (bytes_left > 0) fwrite(pos + 1, 1, bytes_left, sink);
        }
    }

    return bytes_recv;
}