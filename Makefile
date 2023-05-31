
SRCS := $(wildcard storage/*.c)
OBJS := $(SRCS:.c=.o)
HDRS := $(wildcard storage/*.h)

CFLAGS := -g `pkg-config fuse --cflags`
LDLIBS := `pkg-config fuse --libs`

nufs: $(OBJS) nufs.o
	gcc $(CLFAGS) -o $@ $^ $(LDLIBS)

nufs_ll: $(OBJS) nufs_ll.o
	gcc $(CLFAGS) -o $@ $^ $(LDLIBS)

%.o: %.c $(HDRS)
	gcc $(CFLAGS) -c -o $@ $<

clean: unmount
	rm -f nufs *.o test.log data.nufs
	rmdir mnt || true

mount: nufs
	mkdir -p mnt || true
	./nufs -s -f mnt data.nufs

unmount:
	fusermount -u mnt || true

test: nufs
	perl test.pl

gdb: nufs
	mkdir -p mnt || true
	gdb --args ./nufs -s -f mnt data.nufs

.PHONY: clean mount unmount gdb

