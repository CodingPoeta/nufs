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

#define FUSE_USE_VERSION 34
#include <fuse3/fuse_lowlevel.h>

// implementation for: man 2 access
// Checks if a file exists.
void nufs_access(fuse_req_t req, fuse_ino_t ino, int mask) {
  inode_t *node = get_inode(ino);

  printf("--------------------\n");
  printf("access(%ld, %04o) -> \n", ino, mask);
  if (node) fuse_reply_err(req, 0);
  else fuse_reply_err(req, ENOENT);
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
void nufs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  printf("getting stats of inode at %ld\n", ino);

  // get inode with inum
  inode_t *node = get_inode(ino);

  struct stat* st = malloc(sizeof(struct stat));
  // fill out stat struct
  memset(st, 0, sizeof(stat));
  st->st_uid = getuid();
  st->st_mode = node->mode;
  st->st_size = node->size;
  st->st_nlink = node->refs;

  printf("--------------------\n");
  printf("getting attr\n");
  fuse_reply_attr(req, st, 1.0);
}

// implementation for: man 2 readdir
// lists the contents of a directory
void nufs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
			 struct fuse_file_info *fi) {
  int rv = 0;
  dirent_node_t *items = storage_list(NULL, ino);
  int flag = 0;

  for (dirent_node_t *xs = items; xs != 0;) {
    struct stat st;
    printf("current item: %s\n", xs->entry.name);

    rv = storage_stat(NULL, xs->entry.inum, &st);
    assert(rv == 0);
    fuse_add_direntry(req, NULL, 0, xs->entry.name, &st, (off_t)0);

    dirent_node_t *to_del = xs;
    xs = to_struct((list_next(&xs->dirent_list)), dirent_node_t, dirent_list);
    list_del(&to_del->dirent_list);
    free(to_del);

    if (to_del == xs) {
      break;
    }
  }

  printf("--------------------\n");
  printf("readdir(%ld) -> %d\n", ino, rv);
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
void nufs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode, dev_t rdev) {
  printf("making object\n");
  int rv = storage_mknod(NULL, name, parent, mode);

  printf("--------------------\n");
  printf("mknod(%s, %04o) -> %d\n", name, mode, rv);
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
void nufs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode) {
  nufs_mknod(req, parent, name, mode | 040000, 0);
  
  inode_t *pnode = get_inode(parent);
  int inum = directory_lookup(pnode, name);
  inode_t *node = get_inode(inum);

  directory_put(node, ".", inum);
  directory_put(node, "..", parent);

  printf("making dir\n");

  printf("--------------------\n");
  printf("mkdir(%s) -> %d\n", name, inum);
}

// unlinks file from this path
void nufs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
  int rv = storage_unlink(NULL, name, parent);
  printf("--------------------\n");
  printf("unlink(%s) -> %d\n", name, rv);
}

// links the files from the to paths
void nufs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
		      const char *newname) {
  int rv = storage_link(NULL, ino, NULL, newparent, newname);
  printf("--------------------\n");
  printf("link(%ld %ld/%s) -> %d\n", ino, newparent, newname, rv);
}

// removes the directory from that path
void nufs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
  inode_t *pnode = get_inode(parent);
  int inum = directory_lookup(pnode, name);
  inode_t *node = get_inode(inum);

  int mode = node->mode;

  printf("--------------------\n");
  if (mode != 040755) {
    printf("rmdir(%s) -> %d\n", name, -1);
    return;
  }

  nufs_unlink(req, parent, name);
}

// implements: man 2 rename
// called to move a file within the same filesystem
void nufs_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
			fuse_ino_t newparent, const char *newname,
			unsigned int flags) {
  int rv = storage_rename(NULL, parent, name, NULL, newparent, newname);
  printf("--------------------\n");
  printf("rename(%s => %ld %s) -> %d\n", name, newparent, newname, rv);
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
void nufs_open(fuse_req_t req, fuse_ino_t ino,
		      struct fuse_file_info *fi) {
  inode_t *node = get_inode(ino);
  if (node) fuse_reply_err(req, 0);
  else fuse_reply_open(req, fi);

  printf("--------------------\n");
  printf("open(%ld)\n", ino);
}

// Actually read data
void nufs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		      struct fuse_file_info *fi) {
  char* buf = malloc(size);
  int rv = storage_read(NULL, ino, buf, size, off);
  if (rv == 0) {
    fuse_reply_buf(req, buf, size);
    // fuse_reply_data(req, &buf, FUSE_BUF_SPLICE_MOVE);
  } else {
    fuse_reply_err(req, ENOENT);
  }
  printf("--------------------\n");
  printf("read(%ld, %ld bytes, @+%ld) -> %d\n", ino, size, off, rv);
}

// Actually write data
void nufs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		       size_t size, off_t off, struct fuse_file_info *fi) {
  int rv = storage_write(NULL, ino, buf, size, off);
  printf("--------------------\n");
  printf("write(%ld, %ld bytes, @+%ld) -> %d\n", ino, size, off, rv);
}

// Extended operations
void nufs_ioctl(fuse_req_t req, fuse_ino_t ino, int cmd,
		       void *arg, struct fuse_file_info *fi, unsigned flags,
		       const void *in_buf, size_t in_bufsz, size_t out_bufsz) {
  int rv = 0;
  printf("--------------------\n");
  printf("ioctl(%ld, %d, ...) -> %d\n", ino, cmd, rv);
}

static const struct fuse_lowlevel_ops nufs_ops = {
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
  .open = nufs_open,
  .read = nufs_read,
  .write = nufs_write,
  .ioctl = nufs_ioctl,
};

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);

  // initalize blocks
  storage_init(argv[--argc]);

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *se;
	struct fuse_cmdline_opts opts;
	struct fuse_loop_config config;
	int ret = -1;

	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;
	if (opts.show_help) {
		printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		ret = 0;
		goto err_out1;
	} else if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		ret = 0;
		goto err_out1;
	}

	if(opts.mountpoint == NULL) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		printf("       %s --help\n", argv[0]);
		ret = 1;
		goto err_out1;
	}

	se = fuse_session_new(&args, &nufs_ops,
			      sizeof(nufs_ops), NULL);
	if (se == NULL)
	    goto err_out1;

	if (fuse_set_signal_handlers(se) != 0)
	    goto err_out2;

	if (fuse_session_mount(se, opts.mountpoint) != 0)
	    goto err_out3;

	fuse_daemonize(opts.foreground);

	/* Block until ctrl+c or fusermount -u */
	if (opts.singlethread) {
    printf("singlethread loop start\n");
		ret = fuse_session_loop(se);
	} else {
    printf("multithread loop start\n");
		config.clone_fd = opts.clone_fd;
		config.max_idle_threads = opts.max_idle_threads;
		ret = fuse_session_loop_mt(se, &config);
	}

	fuse_session_unmount(se);
err_out3:
	fuse_remove_signal_handlers(se);
err_out2:
	fuse_session_destroy(se);
err_out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);

	return ret ? 1 : 0;
}
