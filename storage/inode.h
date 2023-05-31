// Inode manipulation routines.
//
// Feel free to use as inspiration.

// based on cs3650 starter code

#ifndef INODE_H
#define INODE_H

#include <time.h>

#include "bitmap.h"
#include "blocks.h"

#define ROOT_INODE 1

typedef struct inode {
  int refs;   // reference count
  int mode;   // permission & type
  int size;   // bytes
  int block;  // single block pointer (if max file size <= 4K)
  int next_inode;  // indirect blocks (inode list)
  time_t atime;
  time_t mtime;
} inode_t;

// pretty print inode
void print_inode(inode_t *node);

// get inode at given inum
inode_t *get_inode(int inum);

// allocates next free inode, return inum of allocated inode
int alloc_inode();

// free inode at inum
void free_inode(int inum);

// grow inode by size
int grow_inode(inode_t *node, int size);

// shrink inode by size
int shrink_inode(inode_t *node, int size);

#endif
