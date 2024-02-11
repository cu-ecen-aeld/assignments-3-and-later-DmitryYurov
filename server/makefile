.PHONY: clean all aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -c -o aesdsocket.o aesdsocket.c
	$(CC) $(CFLAGS) -I/ aesdsocket.o -o aesdsocket

default: aesdoscket

all: aesdsocket

clean:
	rm -f *.o aesdsocket *.elf *.map
