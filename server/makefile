.PHONY: clean all aesdsocket

CC ?= gcc
LDFLAGS ?= -lpthread -lrt
CFLAGS ?= -g -Wall -Werror

aesdsocket: aesdsocket.c recv_send.c conn_threads.c timer_utils.c
	$(CC) $(CFLAGS) -c -o aesdsocket.o aesdsocket.c
	$(CC) $(CFLAGS) -c -o recv_send.o recv_send.c
	$(CC) $(CFLAGS) -c -o conn_threads.o conn_threads.c
	$(CC) $(CFLAGS) -I/ aesdsocket.o recv_send.o conn_threads.o -o aesdsocket $(LDFLAGS)

default: aesdoscket

all: aesdsocket

clean:
	rm -f *.o aesdsocket *.elf *.map
