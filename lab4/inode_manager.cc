#include "inode_manager.h"

#define DB 1

// disk layer -----------------------------------------
static int my_time = 0;

disk::disk()
{
  bzero(blocks, sizeof(blocks));
  // memset(blocks[2], 0xFF, INODE_NUM / 8 + BLOCK_NUM / BPB / 8);
  // memset(blocks[2] + INODE_NUM / 8 + BLOCK_NUM / BPB / 8, 0xF, 1);
}
// 2 + 8 + 1 + 1024

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  // magic number, i am too lazy to calculate the inode block number 
  // and i think 1050 may be enough.

  for (int i=1050;i<BLOCK_NUM;i+=1){
    if (using_blocks[i] == 0){
      using_blocks[i] = 1;
      return i;
    }
  }
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  assert(0);
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  using_blocks[id] = 0;
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  char buf[BLOCK_SIZE];
  struct inode * ino_disk;

  for (int j=1;j<=INODE_NUM;j++){
    if (using_ino[j] == 0){
      using_ino[j] = 1;
      ino_disk = (struct inode*)buf + (j)%IPB;
      struct inode n;
      n.type = type;
      n.ctime = my_time++;
      n.atime = 0;
      n.mtime = 0;
      n.size = 0;
      *ino_disk = n;
      bm->write_block(IBLOCK(j, bm->sb.nblocks), buf);
      return j;
    }
  }
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  using_ino[inum] = 0;
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  struct inode * ino = get_inode(inum);
  if (ino->type == 0)
    return;
  // fprintf(stderr, "read file: %d size: %d\n", inum, ino->size);
  int read_n = 0;
  int blocks = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  char * buf = (char *) malloc(blocks * BLOCK_SIZE);
  int NIN[NINDIRECT];
  if (blocks > NDIRECT)
    bm->read_block(ino->blocks[NDIRECT], (char *)NIN);
  for (int i=0;i<blocks;i++){
    if (i < NDIRECT){
      bm->read_block(ino->blocks[i], buf + read_n);
      // fprintf(stderr, "block read: %d\n", ino->blocks[i]);
    }else{
      bm->read_block(NIN[i-NDIRECT], buf + read_n);
    }
    read_n += BLOCK_SIZE;
  }
  *buf_out=buf;
  *size=ino->size;
  ino->atime++;
  put_inode(inum, ino);
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  struct inode * ino = get_inode(inum);
  if (ino->type == 0)
    return;
  // fprintf(stderr, "write file: %d size: %d inum size \n", inum, size, ino->size);
  int write_n = 0;
  char zero[BLOCK_SIZE];
  bzero(zero, sizeof(zero));
  int NIN[NINDIRECT];
  
  int blocks = size / BLOCK_SIZE + (size % BLOCK_SIZE > 0);
  int blocks_h = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  if (blocks > MAXFILE){
    return;
  }
  if (ino->size <= size){
    if (blocks <= NDIRECT){
      for (int i=blocks_h;i<blocks;i++){
        ino->blocks[i] = bm->alloc_block();
        // fprintf(stderr, "block: %d\n", ino->blocks[i]);
      }
    }else{
      if (blocks_h <= NDIRECT)
        ino->blocks[NDIRECT] = bm->alloc_block();
      bm->read_block(ino->blocks[NDIRECT], (char *)NIN);
      int i=0;
      for (i=blocks_h;i<NDIRECT;i++){
        ino->blocks[i] = bm->alloc_block();
      }
      for (i;i<blocks;i++){
        NIN[i-NDIRECT] = bm->alloc_block();
      }
      // fprintf(stderr, "block end: %d\n", i);
      bm->write_block(ino->blocks[NDIRECT], (char *) NIN);
    }
  }else{
    if (blocks_h <= NDIRECT){
      for (int i=blocks;i<blocks_h;i++){
        bm->free_block(ino->blocks[i]);
        // fprintf(stderr, "block free: %d\n", ino->blocks[i]);
      }
    }else{
      bm->read_block(ino->blocks[NDIRECT], (char *)NIN);
      int i=0;
      for (i=blocks;i<NDIRECT;i++){
        bm->free_block(ino->blocks[i]);
      }
      for (i;i<blocks_h;i++){
        bm->free_block(NIN[i-NDIRECT]);
      }
      // fprintf(stderr, "block free end: %d\n", i);
      if (blocks <= NDIRECT){
        bm->free_block(ino->blocks[NDIRECT]);
        ino->blocks[NDIRECT] = 0;
      }
    }
  }

  for (int i=0; i<blocks; i++){
      if (i >= NDIRECT){
        bm->write_block(NIN[i-NDIRECT], zero);
        bm->write_block(NIN[i-NDIRECT], buf + write_n);
      }else{
        bm->write_block(ino->blocks[i], zero);
        bm->write_block(ino->blocks[i], buf + write_n);
      }
      write_n += BLOCK_SIZE;
  }

  ino->mtime = my_time;
  ino->ctime = my_time++;
  ino->size = size;
  put_inode(inum, ino);

  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode * i = get_inode(inum);
  if (i == NULL)
    return;
  a.atime = i->atime;
  a.ctime = i->ctime;
  a.mtime = i->mtime;
  a.size = i->size;
  a.type = i->type;
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  struct inode * ino = get_inode(inum);
  if (ino->type == 0)
    return;
  
  // if (ino->type == extent_protocol::T_DIR){
  //   assert(0);
  // }
  
  int NIN[NINDIRECT];
  
  int blocks = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  
  if (blocks <= NDIRECT){
    for (int i=0;i<blocks;i++){
      bm->free_block(ino->blocks[i]);
    }
  }else{
    bm->read_block(ino->blocks[NDIRECT], (char *)NIN);
    int i;
    for (i=0;i<NDIRECT;i++){
      bm->free_block(ino->blocks[i]);
    }
    for (i;i<blocks;i++){
      bm->free_block(NIN[i-NDIRECT]);
    }
    bm->free_block(ino->blocks[NDIRECT]);
  }
  ino->type = 0;
  put_inode(inum, ino);
  free_inode(inum);
  return;
}

void
inode_manager::append_block(uint32_t inum, blockid_t &bid)
{
  #if DB
  std::cout << "append:" << inum << " " << bid << std::endl;
  #endif

  bid = bm->alloc_block();
  struct inode * ino = get_inode(inum);

  int blocks = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  if (blocks > MAXFILE){
    return;
  }
  if (blocks < NDIRECT){
    ino->blocks[blocks] = bid;
  }else{
    int NIN[NINDIRECT];
    if (blocks == NDIRECT){
      ino->blocks[NDIRECT] = bm->alloc_block();
    }
    bm->read_block(ino->blocks[NDIRECT], (char *)NIN);
    NIN[blocks-NDIRECT] = bid;
    bm->write_block(ino->blocks[NDIRECT], (char *) NIN);
  }
  blocks++;
  ino->size += BLOCK_SIZE;
  put_inode(inum, ino);
}

void
inode_manager::get_block_ids(uint32_t inum, std::list<blockid_t> &block_ids)
{
  #if DB
  std::cout << "get blockids:" << inum << std::endl;
  #endif

  struct inode * ino = get_inode(inum);
  if (ino->type == 0)
    return;
  
  int NIN[NINDIRECT];
  int blocks = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  
  if (blocks <= NDIRECT){
    for (int i=0;i<blocks;i++){
      block_ids.push_back(ino->blocks[i]);
    }
  }else{
    bm->read_block(ino->blocks[NDIRECT], (char *)NIN);
    for (int i=0;i<NDIRECT;i++){
      block_ids.push_back(ino->blocks[i]);
    }
    for (int i=NDIRECT;i<blocks;i++){
      block_ids.push_back(NIN[i-NDIRECT]);
    }
  }
  return;
}

void
inode_manager::read_block(blockid_t id, char buf[BLOCK_SIZE])
{
  bm->read_block(id, buf);
  /*
   * your code goes here.
   */

}

void
inode_manager::write_block(blockid_t id, const char buf[BLOCK_SIZE])
{
  bm->write_block(id, buf);
  /*
   * your code goes here.
   */
}

void
inode_manager::complete(uint32_t inum, uint32_t size)
{
  #if DB
  std::cout << "complete:" << inum << " " << size << std::endl;
  #endif

  struct inode * ino = get_inode(inum);
  ino->size = size;
  put_inode(inum, ino);
  /*
   * your code goes here.
   */
}

