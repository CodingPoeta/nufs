// Directory manipulation functions.
//
// Feel free to use as inspiration.

// based on cs3650 starter code

#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME_LENGTH 48

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "blocks.h"
#include "inode.h"
#include "slist.h"
#include "list.h"

typedef struct dirent {
  char name[DIR_NAME_LENGTH];
  int inum;
  char _reserved[12];
  int filled;
} dirent_t;

typedef struct dirent_node {
  dirent_t entry;
  list_entry_t dirent_list;
} dirent_node_t;

// Initializes the root node directory
void directory_init();

// Find the inode of the file in the passed in directory
int directory_lookup(inode_t *dd, const char *name);

// Looks for the inode at the end of the path passed in
int tree_lookup(const char *path);

// Puts the file and it's inode within the directory
int directory_put(inode_t *dd, const char *name, int inum);

// deletes the file name within the passed in directory
int directory_delete(inode_t *dd, const char *name);

// gets an dirent_node struct of each file dirent at the end of the passed in path
dirent_node_t *directory_list(const char *path, int inum);

// prints everything inside the passed in directory
void print_directory(inode_t *dd);

#endif
