#include "storage.h"

// initialize storage
void storage_init(const char *path) {
  // initialize blocks and directory
  printf("initalizing storage\n");
  blocks_init(path);
  directory_init();
}

// get objects stats, returns something other than zero if it doesn't work
int storage_stat(const char *path, int inum, struct stat *st) {
  // get inum and make sure its valid
  if(path) inum = tree_lookup(path);

  if (inum < 0) {
    return -1;
  }
  printf("getting stats of inode at %d\n", inum);

  // get inode with inum
  inode_t *node = get_inode(inum);

  // fill out stat struct
  memset(st, 0, sizeof(stat));
  st->st_uid = getuid();
  st->st_mode = node->mode;
  st->st_size = node->size;
  st->st_nlink = node->refs;

  return 0;
}

// read data from object into buf starting at an offset
int storage_read(const char *path, int inum,  char *buf, size_t size, off_t offset) {
  if (path) inum = tree_lookup(path);

  printf("reading from inode at %d\n", inum);

  inode_t *node = get_inode(inum);

  // make sure offset and size are >= 0
  assert(offset >= 0);
  assert(size >= 0);

  // copy size bytes to buf from path + offset
  if (size == 0) {
    return 0;
  } else {
    if (offset + size > node->size) {
      size = node->size - offset;
    }
    inode_t* pnode = node;

    while (offset >= BLOCK_SIZE) {
      pnode = get_inode(pnode->next_inode);
      offset -= BLOCK_SIZE;
    }
    void *block_temp = blocks_get_block(pnode->block);
    block_temp += offset;

    if (size + offset > BLOCK_SIZE) {
      memcpy(buf, block_temp, BLOCK_SIZE - offset);
      int temp_sz = size;
      temp_sz -= BLOCK_SIZE - offset;
      buf += BLOCK_SIZE - offset;
      while (temp_sz > BLOCK_SIZE) {
        pnode = get_inode(pnode->next_inode);
        block_temp = blocks_get_block(pnode->block);
        memcpy(buf, block_temp, BLOCK_SIZE);
        buf += BLOCK_SIZE;
        temp_sz -= BLOCK_SIZE;
      }
      pnode = get_inode(pnode->next_inode);
      block_temp = blocks_get_block(pnode->block);
      memcpy(buf, block_temp, temp_sz);
    } else {
      memcpy(buf, block_temp, size);
    }

    return size;
  }
}

// write from path to buff starting at an offset
int storage_write(const char *path, int inum, const char *buf, size_t size,
                  off_t offset) {
  if (path) inum = tree_lookup(path);
  assert(inum >= 0);
  inode_t *node = get_inode(inum);

  printf("writing from inode at %d\n", inum);

  assert(offset >= 0);
  assert(size >= 0);

  // copy size bytes from buf to path + offset
  if (size == 0) {
    return 0;
  } else {
    if (size + offset > node->size) {
      if (grow_inode(node, size + offset - node->size) < 0) {
        return -1;
      }
    }
    inode_t* pnode = node;
    while (offset >= BLOCK_SIZE) {
      pnode = get_inode(pnode->next_inode);
      offset -= BLOCK_SIZE;
    }
    void *block_temp = blocks_get_block(pnode->block);
    block_temp += offset;

    if (size + offset > BLOCK_SIZE) {
      memcpy(block_temp, buf, BLOCK_SIZE - offset);
      int temp_sz = size;
      temp_sz -= BLOCK_SIZE - offset;
      buf += BLOCK_SIZE - offset;
      while (temp_sz > 0) {
        pnode = get_inode(pnode->next_inode);
        block_temp = blocks_get_block(pnode->block);
        memcpy(block_temp, buf, temp_sz > BLOCK_SIZE ? BLOCK_SIZE : temp_sz);
        temp_sz -= BLOCK_SIZE;
        buf += BLOCK_SIZE;
      }
    } else {
      memcpy(block_temp, buf, size);
    }

    return size;
  }
}

// truncate file to size
int storage_truncate(const char *path, off_t size) {
  // get inum and ensure it's valid
  int inum = tree_lookup(path);
  assert(inum >= 0);

  // get inode at inum and its size
  inode_t *node = get_inode(inum);
  int node_size = node->size;

  printf("truncating inode at %d\n", inum);

  // grow inode if the size is greater than the inode's current size
  if (size >= node->size) {
    int rv = grow_inode(node, size - node_size);
    return rv;
  }
  // shrink inode if the size is less than the inode's curent size
  else {
    int rv = shrink_inode(node, node_size - size);
    return rv;
  }
}

// make object at path
int storage_mknod(const char *path, const char *name, int pinum, int mode) {
  // make sure it doesn't already exist
  if (path) {
    pinum = tree_lookup(path);
  }
  
  assert(pinum >= 0);
  inode_t *directory_node = get_inode(pinum);
  int inum = directory_lookup(directory_node, name);
  if (inum >= 0) {
    return -EEXIST;
  }

  inum = alloc_inode();
  inode_t *node = get_inode(inum);
  node->refs = 1;
  node->mode = mode;
  node->size = 0;

  printf("creating object for inode %d\n", inum);

  directory_put(directory_node, name, inum);
  printf("mknod(%s %s, %04o) -> %d\n", path, name, mode, 0);

  return 0;
}

// unlink object at path
int storage_unlink(const char *path, const char *name, int pinum) {
  printf("unlinking\n");

  // get directory inode
  if (path) pinum = tree_lookup(path);
  inode_t *directory_node = get_inode(pinum);

  // unlink the child from the directory
  int inum = directory_lookup(directory_node, name);
  inode_t *node = get_inode(inum);
  node->refs--;
  int rv = directory_delete(directory_node, name);

  // free inode
  // int inum = tree_lookup(path);
  if (node->refs <= 0) {
    free_inode(inum);
  }

  return rv;
}

// create link between from and to
int storage_link(const char *from, int from_inum, const char *to_parent, const char *to_child) {
  // get inum and ensure validity
  if (from) from_inum = tree_lookup(from);
  assert(from_inum >= 0);

  printf("linking");

  inode_t *node = get_inode(from_inum);
  node->refs++;

  // get parent inode
  int to_parent_inum = tree_lookup(to_parent);
  inode_t *to_parent_node = get_inode(to_parent_inum);

  // create link
  int rv = directory_put(to_parent_node, to_child, from_inum);

  // return status
  return rv;
}

// rename from to to
int storage_rename(const char *from_parent, int from_pinum, const char *from_child, const char *to_parent, const char *to_child) {
  if (from_parent) from_pinum = tree_lookup(from_parent);

  inode_t* from_pnode = get_inode(from_pinum);
  int from_inum = directory_lookup(from_pnode, from_child);
  
  storage_link(NULL, from_inum, to_parent, to_child);
  storage_unlink(from_parent, from_child, -1);

  printf("renaming");
  return 0;
}

// list objects at path
dirent_node_t *storage_list(const char *path, int inum) {
  printf("listing\n");
  return directory_list(path, inum);
}

// retrieve the parent dir of the path, mutates directory
void split_path(const char *path, char *directory, char *name) {
  strcpy(directory, path);

  // start at end of path, iterate until /
  int counter = 0;
  int index = strlen(path) - 1;
  while (path[index] != '/') {
    counter++;
    index--;
  }

  // add null terminator at end of parent
  if (counter != strlen(path) - 1) {
    directory[strlen(path) - counter - 1] = '\0';
    if (name) strcpy(name, &directory[strlen(path) - counter]);
  }
  // no parent directory, add null terminator
  else {
    directory[1] = '\0';
    if (name) strcpy(name, &path[1]);
  }
}
