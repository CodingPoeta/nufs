// based on cs3650 starter code

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "storage/directory.h"
#include "storage/inode.h"
#include "storage/storage.h"

#define FUSE_USE_VERSION 30
#include <fuse.h>

// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char *path, int mask) {
  printf("----------------start access----------------\n");
  int inum = tree_lookup(path);

  if (inum == -1) {
    return -1;
  }

  inode_t *node = get_inode(inum);

  printf("access(%s, %04o) -> \n", path, mask);
  if (node) {
    return 0;
  } else {
    return -1;
  }
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
int nufs_getattr(const char *path, struct stat *st) {
  printf("----------------start getattr----------------\n");
  int rv = storage_stat(path, -1, st);

  if (rv < 0) {
    return -ENOENT;
  }
  printf("getting attr\n");
  return rv;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  printf("----------------start readdir----------------\n");
  int rv = 0;
  dirent_node_t *items = storage_list(path, -1);
  int flag = 0;

  for (dirent_node_t *xs = items; xs != 0;) {
    struct stat st;
    printf("current item: %s\n", xs->entry.name);

    rv = storage_stat(NULL, xs->entry.inum, &st);
    assert(rv == 0);
    filler(buf, xs->entry.name, &st, 0);

    dirent_node_t *to_del = xs;
    xs = to_struct((list_next(&xs->dirent_list)), dirent_node_t, dirent_list);
    list_del(&to_del->dirent_list);
    free(to_del);

    if (to_del == xs) {
      break;
    }
  }

  // filler(buf, "hello.txt", &st, 0);

  printf("readdir(%s) -> %d\n", path, rv);
  return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
  printf("----------------start mknod----------------\n");

  char *directory = malloc(strlen(path) + 1);
  char *name = malloc(strlen(path) + 1);
  split_path(path, directory, name);
  int rv = storage_mknod(directory, name, -1, mode);

  free(directory);
  free(name);

  printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode) {
  printf("----------------start mkdir----------------\n");

  int rv = nufs_mknod(path, mode | 040000, 0);
  int inum = tree_lookup(path);
  inode_t *node = get_inode(inum);
  char *from_parent = malloc(strlen(path) + 1);
  split_path(path, from_parent, NULL);
  int parent_inum = tree_lookup(from_parent);
  
  free(from_parent);

  if (rv >= 0) {
    directory_put(node, ".", inum);
    directory_put(node, "..", parent_inum);
  }

  printf("mkdir(%s) -> %d\n", path, rv);
  return rv;
}

// unlinks file from this path
int nufs_unlink(const char *path) {
  printf("----------------start unlink----------------\n");

  char *directory = malloc(strlen(path) + 1);
  char *child = malloc(strlen(path) + 1);
  split_path(path, directory, child);

  int rv = storage_unlink(directory, -1, child);

  // free allocated memory
  free(directory);
  free(child);

  printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

// links the files from the to paths
int nufs_link(const char *from, const char *to) {
  printf("----------------start link----------------\n");

  char *to_parent = malloc(strlen(to) + 1);
  char *to_child = malloc(strlen(to) + 1);
  
  // get parent and child
  split_path(to, to_parent, to_child);

  int rv = storage_link(from, -1, to_parent, -1, to_child);
  printf("link(%s => %s) -> %d\n", from, to, rv);

  free(to_parent);
  free(to_child);

  return rv;
}

// removes the directory from that path
int nufs_rmdir(const char *path) {
  printf("----------------start rmdir----------------\n");

  int inum = tree_lookup(path);
  inode_t *node = get_inode(inum);

  int mode = node->mode;

  if (mode != 040755) {
    printf("rmdir(%s) -> %d\n", path, -1);
    return -1;
  }

  return nufs_unlink(path);
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to) {
  printf("----------------start rename----------------\n");

  char *from_parent = malloc(strlen(from) + 1);
  char *from_child = malloc(strlen(from) + 1);
  char *to_parent = malloc(strlen(to) + 1);
  char *to_child = malloc(strlen(to) + 1);

  // get parent and child
  split_path(from, from_parent, from_child);
  split_path(to, to_parent, to_child);

  int rv = storage_rename(from_parent, -1, from_child, to_parent, -1, to_child);
  printf("rename(%s => %s) -> %d\n", from, to, rv);

  free(from_parent);
  free(from_child);
  free(to_parent);
  free(to_child);

  return rv;
}

// changes permissions
int nufs_chmod(const char *path, mode_t mode) {
  printf("----------------start chmod----------------\n");

  int rv = -1;
  int inum = tree_lookup(path);
  inode_t *node = get_inode(inum);

  if (node->mode == mode) {
    return 0;
  } else {
    node->mode = mode;
    rv = 0;
  }
  printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

// truncates file/dir by the passed in size
int nufs_truncate(const char *path, off_t size) {
  printf("----------------start truncate----------------\n");
  int rv = storage_truncate(path, size);
  printf("truncate(%s, %ld bytes) -> %d\n", path, size, rv);
  return rv;
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char *path, struct fuse_file_info *fi) {
  printf("----------------start open----------------\n");
  int rv = nufs_access(path, 0);
  printf("open(%s) -> %d\n", path, rv);
  return rv;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  printf("----------------start read----------------\n");
  int rv = storage_read(path, -1, buf, size, offset);
  printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  printf("----------------start write----------------\n");
  int rv = storage_write(path, -1, buf, size, offset);
  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char *path, const struct timespec ts[2]) {
  printf("----------------start utimens----------------\n");
  int inum = tree_lookup(path);
  inode_t *node = get_inode(inum);
  time_t time = ts->tv_sec;
  node->mtime = time;
  if (node == NULL) {
    return -1;
  }
  printf("utimens(%s, [%ld, %ld; %ld, %ld]) -> %d\n", path, ts[0].tv_sec,
         ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, 1);
  return 0;
}

// Extended operations
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
  printf("----------------start ioctl----------------\n");
  int rv = 0;
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

static const struct fuse_operations nufs_ops = {
  .access = nufs_access,
  .getattr = nufs_getattr,
  .readdir = nufs_readdir,
  .mknod = nufs_mknod,
  // .create   = nufs_create, // alternative to mknod
  .mkdir = nufs_mkdir,
  .link = nufs_link,
  .unlink = nufs_unlink,
  .rmdir = nufs_rmdir,
  .rename = nufs_rename,
  .chmod = nufs_chmod,
  .truncate = nufs_truncate,
  .open = nufs_open,
  .read = nufs_read,
  .write = nufs_write,
  .utimens = nufs_utimens,
  .ioctl = nufs_ioctl,
};

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);

  // initalize blocks
  storage_init(argv[--argc]);
  return fuse_main(argc, argv, &nufs_ops, NULL);
}
