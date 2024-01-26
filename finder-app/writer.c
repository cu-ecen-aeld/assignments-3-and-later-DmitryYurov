#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char** argv) {
    FILE* fptr = NULL;
    int last_error = 0;

    openlog(argv[0], LOG_CONS, LOG_USER);
    if (argc != 3) {
        last_error = 1;
        syslog(LOG_ERR, "Wrong number of arguments: %d\n", argc);
        return last_error;
    }

    fptr = fopen(argv[1], "a");
    if (fptr == NULL) {
        last_error = errno;
        syslog(LOG_ERR, "Failed to open the file with path %s, error: %s\n", argv[1], strerror(errno));
        return last_error;
    }

    syslog(LOG_DEBUG, "Writing message %s to the file %s\n", argv[2], argv[1]);

    if (fprintf(fptr, "%s\n", argv[2]) < 0) {
        last_error = errno;
        syslog(LOG_ERR, "Failed to write into the file, reason: %s\n", strerror(errno));
    }

    fclose(fptr);
    return 0;
}