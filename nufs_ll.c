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

void nufs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name){
  printf("----------------start lookup: ino=%ld, name=%s\n", parent, name);
  struct fuse_entry_param e;

  inode_t *pnode = get_inode(parent);
  int ino = directory_lookup(pnode, name);
  if (ino < 0) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  inode_t *node = get_inode(ino);
  memset(&e, 0, sizeof(e));
  e.ino = ino;
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;
  int rv = storage_stat(NULL, ino, &e.attr);
  assert(rv == 0);

  fuse_reply_entry(req, &e);
  printf("+ lookup(%ld, %s) -> %d\n", parent, name, ino);
}

// implementation for: man 2 access
// Checks if a file exists.
void nufs_access(fuse_req_t req, fuse_ino_t ino, int mask) {
  printf("----------------start access: ino=%ld, mask=%04o\n", ino, mask);
  inode_t *node = get_inode(ino);

  if (node) fuse_reply_err(req, 0);
  else fuse_reply_err(req, ENOENT);
  printf("+ access(%ld, %04o) -> \n", ino, mask);
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
void nufs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  printf("----------------start getattr: ino=%ld\n", ino);
  printf("getting stats of inode at %ld\n", ino);

  // get inode with inum
  inode_t *node = get_inode(ino);
  if (node == NULL) {
    fuse_reply_err(req, ENOENT);
    printf("inode %ld not found\n", ino);
    return;
  }

  struct stat st;
  int rv = storage_stat(NULL, ino, &st);
  assert(rv == 0);

  fuse_reply_attr(req, &st, 1.0);
  printf("+ getting attr\n");
}

// implementation for: man 2 readdir
// lists the contents of a directory
void nufs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
			 struct fuse_file_info *fi) {
  printf("----------------start readdir: ino=%ld, size=%ld, off=%ld\n", ino, size, off);
  int rv = 0;
  dirent_node_t *items = storage_list(NULL, ino);
  int flag = 0;

  char *buf;
  char *pbuf;
	size_t rem = size+off;
  off_t next_off = 0;
	buf = calloc(1, rem);
  if (!buf) {
    fuse_reply_err(req, ENOMEM);
    return;
  }
  pbuf = buf;

  for (dirent_node_t *xs = items; xs != 0;) {
    size_t entsize;
    struct stat st;
    printf("current item: %s\n", xs->entry.name);

    rv = storage_stat(NULL, xs->entry.inum, &st);
    assert(rv == 0);
    next_off += fuse_add_direntry(req, NULL, 0,  xs->entry.name, NULL, 0);
    entsize = fuse_add_direntry(req, pbuf, rem, xs->entry.name, &st, next_off);

    dirent_node_t *to_del = xs;
    xs = to_struct((list_next(&xs->dirent_list)), dirent_node_t, dirent_list);
    list_del(&to_del->dirent_list);
    free(to_del);


    pbuf += entsize;
    rem -= entsize;
    if (to_del == xs) {
      break;
    }
  }
  if (next_off == off) {
    fuse_reply_buf(req, NULL, 0);
  } else {
    fuse_reply_buf(req, buf+off, next_off-off);
  }
  free(buf);

  printf("+ readdir(%ld) -> %d\n", ino, rv);
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
void nufs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode, dev_t rdev) {
  printf("----------------start mknod: parent=%ld, name=%s, mode=%04o\n", parent, name, mode);
  int rv = storage_mknod(NULL, name, parent, mode);

  if (rv < 0) {
    fuse_reply_err(req, -rv);
  }
  nufs_lookup(req, parent, name);
  printf("+ mknod(%s, %04o) -> %d\n", name, mode, rv);
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
void nufs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode) {
  printf("----------------start mkdir: parent=%ld, name=%s, mode=%04o\n", parent, name, mode);
  int rv = storage_mknod(NULL, name, parent, mode | 040000);
  if (rv < 0) {
    fuse_reply_err(req, -rv);
  }

  inode_t *pnode = get_inode(parent);
  int inum = directory_lookup(pnode, name);
  inode_t *node = get_inode(inum);

  directory_put(node, ".", inum);
  directory_put(node, "..", parent);

  struct fuse_entry_param e;

  memset(&e, 0, sizeof(e));
  e.ino = inum;
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;

  rv = storage_stat(NULL, inum, &e.attr);
  if (rv < 0) {
    fuse_reply_err(req, -rv);
  }
  fuse_reply_entry(req, &e);

  printf("+ mkdir(%s) -> %d\n", name, inum);
}

// unlinks file from this path
void nufs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
  printf("----------------start unlink: parent=%ld, name=%s\n", parent, name);
  int rv = storage_unlink(NULL, parent, name);
  fuse_reply_err(req, rv == 0 ? 0 : errno);
  printf("unlink(%s) -> %d\n", name, rv);
}

// links the files from the to paths
void nufs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
		      const char *newname) {
  printf("----------------start link: ino=%ld, newparent=%ld, newname=%s\n", ino, newparent, newname);
  int rv = storage_link(NULL, ino, NULL, newparent, newname);
  if (rv < 0) {
    fuse_reply_err(req, -rv);
  }

  struct fuse_entry_param e;

  memset(&e, 0, sizeof(e));
  e.ino = ino;
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;

  rv = storage_stat(NULL, ino, &e.attr);
  if (rv < 0) {
    fuse_reply_err(req, -rv);
  }
  fuse_reply_entry(req, &e);

  printf("+ link(%ld %ld/%s) -> %d\n", ino, newparent, newname, rv);
}

// removes the directory from that path
void nufs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
  printf("----------------start rmdir: parent=%ld, name=%s\n", parent, name);
  inode_t *pnode = get_inode(parent);
  int inum = directory_lookup(pnode, name);
  inode_t *node = get_inode(inum);

  int mode = node->mode;

  if (mode != 040755) {
    printf("+ rmdir(%s) -> %d\n", name, -1);
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  nufs_unlink(req, parent, name);
}

// implements: man 2 rename
// called to move a file within the same filesystem
void nufs_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
			fuse_ino_t newparent, const char *newname,
			unsigned int flags) {
  printf("----------------start rename: parent=%ld, name=%s, newparent=%ld, newname=%s\n", parent, name, newparent, newname);
  int rv = storage_rename(NULL, parent, name, NULL, newparent, newname);
  fuse_reply_err(req, rv == 0 ? 0 : errno);
  printf("rename(%s => %ld %s) -> %d\n", name, newparent, newname, rv);
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
void nufs_open(fuse_req_t req, fuse_ino_t ino,
		      struct fuse_file_info *fi) {
  printf("----------------start open: ino=%ld\n", ino);
  inode_t *node = get_inode(ino);
  if (!node) fuse_reply_err(req, 0);
  else fuse_reply_open(req, fi);

  printf("open(%ld)\n", ino);
}

// Actually read data
void nufs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		      struct fuse_file_info *fi) {
  printf("----------------start read: ino=%ld, size=%ld, off=%ld\n", ino, size, off);
  char* buf = malloc(size);
  int rv = storage_read(NULL, ino, buf, size, off);
  if (rv > 0) {
    fuse_reply_buf(req, buf, size);
    // fuse_reply_data(req, &buf, FUSE_BUF_SPLICE_MOVE);
  } else {
    fuse_reply_err(req, ENOENT);
  }
  printf("read(%ld, %ld bytes, @+%ld) -> %d\n", ino, size, off, rv);
}

// Actually write data
void nufs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		       size_t size, off_t off, struct fuse_file_info *fi) {
  printf("----------------start write: ino=%ld, size=%ld, off=%ld\n", ino, size, off);
  int rv = storage_write(NULL, ino, buf, size, off);

  if (rv >= 0) {
    fuse_reply_write(req, rv);
  } else {
    fuse_reply_err(req, ENOENT);
  }

  printf("write(%ld, %ld bytes, @+%ld) -> %d\n", ino, size, off, rv);
}

// Extended operations
void nufs_ioctl(fuse_req_t req, fuse_ino_t ino, int cmd,
		       void *arg, struct fuse_file_info *fi, unsigned flags,
		       const void *in_buf, size_t in_bufsz, size_t out_bufsz) {
  printf("----------------start ioctl: ino=%ld, cmd=%d\n", ino, cmd);
  int rv = 0;
  fuse_reply_err(req, EBADR);
  printf("ioctl(%ld, %d, ...) -> %d\n", ino, cmd, rv);
}

static const struct fuse_lowlevel_ops nufs_ops = {
  .lookup = nufs_lookup,
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
