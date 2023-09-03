all: sfs
sfs: sfs.o
	gcc sfs.o -o sfs -Wall -D_FILE_OFFSET_BITS=64 -g -pthread -lfuse3 -lrt -ldl
sfs.o: sfs.c
	gcc -Wall `pkg-config fuse3 --cflags --libs` -D_FILE_OFFSET_BITS=64 -g -c -o sfs.o sfs.c
.PHONY: all
clean:
	rm -f sfs sfs.o