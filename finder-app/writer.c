#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char** argv) {
    FILE* fptr = NULL;

    openlog(argv[0], LOG_CONS, LOG_USER);
    if (argc != 3) {
        syslog(LOG_ERR, "Wrong number of arguments: %d\n", argc);
        return 1;
    }

    fptr = fopen(argv[1], "a");
    if (fptr == NULL) {
        syslog(LOG_ERR, "Failed to open the file with path %s, error number: %s\n", argv[1], strerror(errno));
        return 1;
    }

    syslog(LOG_DEBUG, "Writing message %s to the file %s\n", argv[2], argv[1]);
    fprintf(fptr, "%s\n", argv[2]);

    if (errno != 0) {
        syslog(LOG_ERR, "Failed to write into the file, reason: %s\n", strerror(errno));
        fclose(fptr);
        return 1;
    }

    fclose(fptr);
    return 0;
}