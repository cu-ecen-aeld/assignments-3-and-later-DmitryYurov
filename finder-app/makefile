.PHONY: clean all writer

writer: writer.c
	$(CC) $(CFLAGS) -c -o writer.o writer.c
	$(CC) $(CFLAGS) -I/ writer.o -o writer

all: writer

clean:
	rm -f *.o writer *.elf *.map
