CC = gcc
CFLAGS = -g --std=c99 

all: mfs

mfs.o: mfs.c
	$(CC) -c -o mfs.o mfs.c $(CFLAGS)

mfs: mfs.o
	$(CC) -o mfs mfs.o $(CFLAGS)

clean:
	rm -f *.o mfs

.PHONY: all clean