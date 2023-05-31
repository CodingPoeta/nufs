
SRCS := $(wildcard storage/*.c)
OBJS := $(SRCS:.c=.o)
HDRS := $(wildcard storage/*.h)

CFLAGS := -g `pkg-config fuse --cflags`
LDLIBS := `pkg-config fuse --libs`

CFLAGS3 := -g `pkg-config fuse3 --cflags`
LDLIBS3 := `pkg-config fuse3 --libs`

nufs: $(OBJS) nufs.o
	gcc $(CLFAGS) -o $@ $^ $(LDLIBS)

nufs_ll: $(OBJS) nufs_ll.o
	gcc $(CLFAGS3) -o $@ $^ $(LDLIBS3)

%.o: %.c $(HDRS)
	gcc $(CFLAGS) -c -o $@ $<

clean: unmount
	rm -f nufs nufs_ll *.o storage/*.o test.log data.nufs
	rmdir mnt || true

mount: nufs
	mkdir -p mnt || true
	./nufs -s -f mnt data.nufs

unmount:
	fusermount -u mnt || true

mount_ll: nufs_ll
	mkdir -p mnt || true
	./nufs_ll -s -f mnt data.nufs

test: nufs
	perl test.pl

gdb: nufs
	mkdir -p mnt || true
	gdb --args ./nufs -s -f mnt data.nufs

gdb_ll: nufs_ll
	mkdir -p mnt || true
	gdb --args ./nufs_ll -s -f mnt data.nufs

.PHONY: clean mount unmount gdb

