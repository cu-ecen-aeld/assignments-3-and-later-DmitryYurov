.PHONY: clean all aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -g -Wall -Werror -c -o aesdsocket.o aesdsocket.c
	$(CC) $(CFLAGS) -g -Wall -Werror -I/ aesdsocket.o -o aesdsocket

default: aesdoscket

all: aesdsocket

clean:
	rm -f *.o aesdsocket *.elf *.map
