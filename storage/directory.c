#include "directory.h"
#define TOTAL_DIRENTS BLOCK_SIZE / sizeof(dirent_t)

// Initializes the root node directory
void directory_init() {
  int i = ROOT_INODE;
  void *ibm = get_inode_bitmap();
  if (bitmap_get(ibm, i) == 1) {
    printf("root inode already exists\n");
    return;
  }
  bitmap_put(ibm, i, 1);

  inode_t* new_dir_inode = get_inode(i);

  printf("intializing dir\n");
  
  memset(new_dir_inode, 0, sizeof(inode_t));
  new_dir_inode->mode = 040755;
  new_dir_inode->refs = 1;
  new_dir_inode->size = 0;
  new_dir_inode->block = alloc_block();
  new_dir_inode->next_inode = -1;
  new_dir_inode->atime = time(NULL);
  new_dir_inode->mtime = time(NULL);

  directory_put(new_dir_inode, ".", i);
  directory_put(new_dir_inode, "..", i);
}

// Find the inode of the file in the passed in directory
int directory_lookup(inode_t* dd, const char* name) {
  dirent_t* dir_contents = blocks_get_block(dd->block);
  printf("directory lookup: %s\n", name);

  for (int i = 0; i < TOTAL_DIRENTS; i++) {
    if (strcmp(dir_contents[i].name, name) == 0) {
      printf("returning directory inum: %d\n", dir_contents[i].inum);
      return dir_contents[i].inum;
    }
  }
  printf("directory lookup failed\n");
  return -ENOENT;
}

// Looks for the inode at the end of the path passed in
int tree_lookup(const char* path) {
  slist_t* file_path = s_explode(path, '/');
  slist_t* curr_file = file_path;
  int inode_num = ROOT_INODE;
  inode_t* root_inode = get_inode(inode_num);

  printf("tree_lookup path: %s\n", path);

  while (curr_file) {
    if (strcmp(curr_file->data, "") != 0) {
      inode_t* node = get_inode(inode_num);
      inode_num = directory_lookup(node, curr_file->data);
      if (inode_num < 0) {
        return -1;
      }
    }
    curr_file = curr_file->next;
  }

  printf("returning tree lookup num: %d\n", inode_num);
  return inode_num;
}

// Puts the file and it's inode within the directory
int directory_put(inode_t* dd, const char* name, int inum) {
  printf("putting dirs: %s\n", name);
  dirent_t* dir_contents = blocks_get_block(dd->block);

  for (int i = 0; i < TOTAL_DIRENTS; i++) {
    if (dir_contents[i].filled != 1) {
      dir_contents[i].inum = inum;
      strcpy(dir_contents[i].name, name);
      dir_contents[i].filled = 1;
      return 0;
    }
  }
  return -1;
}

// deletes the file name within the passed in directory
int directory_delete(inode_t* dd, const char* name) {
  printf("deleting dirs\n");
  dirent_t* dir_contents = blocks_get_block(dd->block);
  for (int i = 0; i < TOTAL_DIRENTS; i++) {
    if (strcmp(dir_contents[i].name, name) == 0) {
      dir_contents[i].filled = 0;
      return 0;
    }
  }

  return -1;
}

// gets an dirent_node struct of each file name at the end of the passed in path
dirent_node_t *directory_list(const char* path, int inum) {
  printf("listing dirs\n");
  if (path) inum = tree_lookup(path);
  inode_t* dd = get_inode(inum);
  dirent_t* dir_contents = blocks_get_block(dd->block);
  dirent_node_t* dirents = NULL;
  for (int i = 0; i < TOTAL_DIRENTS; i++) {
    if (dir_contents[i].filled == 1) {
      dirent_node_t* tmp = malloc(sizeof(dirent_node_t));
      tmp->entry = dir_contents[i];
      if (!dirents) {
        dirents = tmp;
        list_init(&dirents->dirent_list);
      }
      else list_add_after(&dirents->dirent_list, &tmp->dirent_list);
    }
  }
  return dirents;
}

// prints everything inside the passed in directory
void print_directory(inode_t* dd) {
  dirent_t* dir_contents = blocks_get_block(dd->block);
  printf("printing directory\n");
  for (int i = 0; i < TOTAL_DIRENTS; i++) {
    if (dir_contents[i].filled == 1) {
      printf("-%s\n", dir_contents[i].name);
    }
  }
}
