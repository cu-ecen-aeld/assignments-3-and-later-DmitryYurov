.PHONY: clean all aesdsocket

aesdsocket: aesdsocket.c
	$(CC) -g -Wall -Werror -c -o aesdsocket.o aesdsocket.c
	$(CC) -g -Wall -Werror -c -o recv_send.o recv_send.c
	$(CC) -g -Wall -Werror -I/ aesdsocket.o recv_send.o -o aesdsocket

default: aesdoscket

all: aesdsocket

clean:
	rm -f *.o aesdsocket *.elf *.map
