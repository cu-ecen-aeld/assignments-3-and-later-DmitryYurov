writer: writer.c
	$(CROSS_COMPILE)gcc -g -Wall -c -o writer.o writer.c
	$(CROSS_COMPILE)gcc -g -Wall -I/ writer.o -o writer

.PHONY: clean

clean:
	rm -f *.o writer *.elf *.map