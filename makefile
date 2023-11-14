all: sfs
sfs: sfs.o
	gcc build/sfs.o -o build/sfs -Wall -D_FILE_OFFSET_BITS=64 -g -pthread -lfuse3 -lrt -ldl
sfs.o: sfs.c
	gcc -Wall `pkg-config fuse3 --cflags --libs` -D_FILE_OFFSET_BITS=64 -g -c -o build/sfs.o sfs.c
.PHONY: all
clean:
	rm -f build/sfs build/sfs.o
img:
	dd bs=1K count=8K if=/dev/zero of=sfs.img