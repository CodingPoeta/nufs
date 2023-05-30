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

#define SIZE 64

// pretty print inode
void print_inode(inode_t *node) {
  printf("inode: refs: %d, mode: %d, size: %d, block: %d\n", node->refs,
         node->mode, node->size, node->block);
}

// get inode at given inum
inode_t *get_inode(int inum) {
  // make sure inum is within range
  assert(inum < SIZE);
  void *ibm = get_inode_bitmap();
  if (bitmap_get(ibm, inum) == 0) {
    return NULL;
  }

  printf("getting inode %d\n", inum);
  inode_t *inode_ptr = (inode_t *)blocks_get_block(2);
  return inode_ptr + inum;
}

// allocates next free inode, return inum of allocated inode
int alloc_inode() {
  // get bitmap
  void *ibm = get_inode_bitmap();

  printf("trying to allocate\n");

  // find next free space in the inode bitmap
  for (int i = 0; i < SIZE; i++) {
    // if inode is free in the bitmap
    if (bitmap_get(ibm, i) == 0) {
      // get free inode
      bitmap_put(ibm, i, 1);
      inode_t *node = get_inode(i);

      // allocate memory and fields
      memset(node, 0, sizeof(inode_t));
      node->refs = 0;
      node->mode = 010644;
      node->size = 0;
      node->block = alloc_block();
      node->next_inode = -1;
      node->atime = time(NULL);
      node->mtime = time(NULL);

      // return inum i
      printf("allocating inode at %d\n", i);
      return i;
    }
    // else continue
  }
  // no free inode is found
  return -1;
}

// free inode at inum
void free_inode(int inum) {
  // get bitmap and mark inum as free in bitmap
  void *ibm = get_inode_bitmap();

  // get inode at inum
  inode_t *node = get_inode(inum);

  assert(node->refs == 0);
  printf("freeing inode at %d\n", inum);

  // shrink inode and free block, then set the memory at
  // node to 0s
  shrink_inode(node, node->size);
  free_block(node->block);

  memset(node, 0, sizeof(inode_t));
  bitmap_put(ibm, inum, 0);
}

// grow inode by size
int grow_inode(inode_t *node, int size) {
  int target_size = node->size + size;
  printf("growing inode from %d to %d\n", node->size, target_size);
  node->size = target_size;

  inode_t *pnode = node;
  while (pnode->size > BLOCK_SIZE) {
    if (pnode->next_inode == -1) {
      pnode->next_inode = alloc_inode();
    }
    int sz = pnode->size;
    pnode = get_inode(pnode->next_inode);
    pnode->size = sz - BLOCK_SIZE;
  }

  return 0;
}

// shrink inode by size
int shrink_inode(inode_t *node, int size) {
  void *ibm = get_inode_bitmap();

  inode_t *pnode = node;
  while (size < pnode->size - BLOCK_SIZE) {
    pnode = get_inode(pnode->next_inode);
  }

  int inum_to_del = pnode->next_inode;
  pnode->next_inode = -1;

  inode_t *to_del = get_inode(inum_to_del);
  while (inum_to_del > 0) {
    bitmap_put(ibm, inum_to_del, 0);
    free_block(to_del->block);
    inum_to_del = to_del->next_inode;
    memset(to_del, 0, sizeof(inode_t));
  }

  int target_size = node->size - size;
  printf("shrinking inode from %d to %d\n", node->size, target_size);

  pnode = node;
  pnode->size = target_size;
  while (pnode->next_inode != -1) {
    pnode = get_inode(pnode->next_inode);
    pnode->size -= size;
  }
  return 0;
}
