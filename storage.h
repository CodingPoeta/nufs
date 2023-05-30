// Disk storage manipulation.
//
// Feel free to use as inspiration.

// based on cs3650 starter code

#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "directory.h"
#include "inode.h"
#include "slist.h"

// initialize storage
void storage_init(const char *path);

// get objects stats, returns something other than zero if it doesn't work
int storage_stat(const char *path, int inum, struct stat *st);

// read data from object into buf starting at an offset
int storage_read(const char *path, char *buf, size_t size, off_t offset);

// write from path to buff starting at an offset
int storage_write(const char *path, const char *buf, size_t size, off_t offset);

// truncate file to size
int storage_truncate(const char *path, off_t size);

// make object at path
int storage_mknod(const char *path, int mode);

// unlink object at path
int storage_unlink(const char *path);

// create link between from and to
int storage_link(const char *from, const char *to);

// rename from to to
int storage_rename(const char *from, const char *to);

// list objects at path
dirent_node_t *storage_list(const char *path, int inum);

// retrieve the parent dir of the path, mutates directory
void get_parent_dir(const char *path, char *directory);

// get the child of the path
void get_child(const char *path, char *child);

#endif
