// based on cs3650 starter code

#define _GNU_SOURCE
#include "bitmap.h"
#include "blocks.h"
#include "inode.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int BLOCK_COUNT = 256; // we split the "disk" into 256 blocks
const int BLOCK_SIZE = 4096; // = 4K

const int NUFS_SIZE = BLOCK_SIZE * BLOCK_COUNT; // = 1MB

const int INODE_COUNT = 256; // = 256
const int INODE_SIZE = sizeof(inode_t);

const int BLOCK_BITMAP_SIZE = BLOCK_COUNT / 8;
// Note: assumes block count is divisible by 8
const int INODE_BITMAP_SIZE = INODE_COUNT / 8;

static int blocks_fd = -1;
static void *blocks_base = 0;

// Get the number of blocks needed to store the given number of bytes.
int bytes_to_blocks(int bytes) {
  int quo = bytes / BLOCK_SIZE;
  return bytes % BLOCK_SIZE == 0 ? quo : quo + 1;
}

// Load and initialize the given disk image.
void blocks_init(const char *image_path) {
  blocks_fd = open(image_path, O_CREAT | O_RDWR, 0644);
  assert(blocks_fd != -1);

  // make sure the disk image is exactly 1MB
  int rv = ftruncate(blocks_fd, NUFS_SIZE);
  assert(rv == 0);

  // map the image to memory
  blocks_base =
      mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, blocks_fd, 0);
  assert(blocks_base != MAP_FAILED);

  // block 0 stores the block bitmap
  void *bbm = get_blocks_bitmap();
  bitmap_put(bbm, 0, 1);
  // block 1 stores the inode bitmap
  bitmap_put(bbm, 1, 1);

  // block 2-n stores the inode table
  for (int i = 2; (i-2) * BLOCK_SIZE < INODE_COUNT * INODE_SIZE; ++i) {
    bitmap_put(bbm, i, 1);
  }
}

// Close the disk image.
void blocks_free() {
  int rv = munmap(blocks_base, NUFS_SIZE);
  assert(rv == 0);
}

// Get the given block, returning a pointer to its start.
void *blocks_get_block(int bnum) { return blocks_base + BLOCK_SIZE * bnum; }

// Return a pointer to the beginning of the block bitmap.
// The size is BLOCK_BITMAP_SIZE bytes.
void *get_blocks_bitmap() { return blocks_get_block(0); }

// Return a pointer to the beginning of the inode table bitmap.
void *get_inode_bitmap() {
  // The inode bitmap is stored immediately after the block bitmap
  return blocks_get_block(0) + BLOCK_BITMAP_SIZE;
}

// Allocate a new block and return its index.
int alloc_block() {
  void *bbm = get_blocks_bitmap();

  for (int ii = 1; ii < BLOCK_COUNT; ++ii) {
    if (!bitmap_get(bbm, ii)) {
      bitmap_put(bbm, ii, 1);
      printf("+ alloc_block() -> %d\n", ii);
      return ii;
    }
  }

  return -1;
}

// Deallocate the block with the given index.
void free_block(int bnum) {
  printf("+ free_block(%d)\n", bnum);
  void *bbm = get_blocks_bitmap();
  bitmap_put(bbm, bnum, 0);
}
