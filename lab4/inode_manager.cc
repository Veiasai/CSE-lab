#include "inode_manager.h"

// disk layer -----------------------------------------
static int my_time = 0;

disk::disk()
{
  bzero(blocks, sizeof(blocks));
  memset(blocks[2], 0xFF, INODE_NUM / 8 + BLOCK_NUM / BPB / 8);
  memset(blocks[2] + INODE_NUM / 8 + BLOCK_NUM / BPB / 8, 0xF, 1);
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
  char bitmap[BLOCK_SIZE];
  int bitmap_n= BLOCK_NUM / BPB;
  // skip super block. boot block
  for (int i=0;i<bitmap_n;i+=1){
    d->read_block(i+2, bitmap);
    for (int j=0;j<BPB;j++){
      if ((bitmap[j/8] >> (j % 8)) & 1){
        continue;
      }else{
        // fprintf(stderr, "alloc block j: %d\n", j);
        bitmap[j/8] |= (1 << (j%8));
        d->write_block(i+2, bitmap);
        return i * BPB + j;
      }
    }
  }
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  char bitmap[BLOCK_SIZE];
  d->read_block(BBLOCK(id), bitmap);
  bitmap[(id % BPB) / 8] &= ~(1 << (id % BPB % 8));
  d->write_block(BBLOCK(id), bitmap);
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
  // fprintf(stderr, "alloc inode\n");
  char bitmap[BLOCK_SIZE];
  char buf[BLOCK_SIZE];
  struct inode * ino_disk;
  bm->read_block(2 + BLOCK_NUM / BPB, bitmap);
  for (int j=1;j<=INODE_NUM;j++){
      if ((bitmap[j/8] >> (j % 8)) & 1){
        continue;
      }else{
        bitmap[j/8] |= (1 << (j%8));
        bm->write_block(2 + BLOCK_NUM / BPB, bitmap);
        ino_disk = (struct inode*)buf + (j)%IPB;
        struct inode n;
        n.type = type;
        n.ctime = my_time;
        my_time++;
        n.size = 0;
        *ino_disk = n;
        bm->write_block(IBLOCK(j, bm->sb.nblocks), buf);
        return j;
        // fprintf(stderr, "alloc inode: %d\n", j);
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
  char bitmap[BLOCK_SIZE];
  bm->read_block(2 + BLOCK_NUM / BPB, bitmap);
  if (bitmap[inum/8] & (1 << (inum%8))){
    bitmap[inum/8] ^=  (1 << (inum%8));
    bm->write_block(2 + BLOCK_NUM / BPB, bitmap);
  }
  std::cout << "free inode: " << inum << "\n";
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

  ino->mtime = my_time++;
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
  if (ino->type == extent_protocol::T_DIR){
    char * buf;
    int size;
    read_file(inum, &buf, &size);
    int pos = 0;
    while(pos < size){
        int file_ino = *(uint32_t *)(buf + pos + FILENAME_MAX);
        remove_file(file_ino);
        pos += FILENAME_MAX + sizeof(uint32_t);
    }
  }
    
  int NIN[NINDIRECT];
  
  int blocks = ino->size / BLOCK_SIZE + (ino->size % BLOCK_SIZE > 0);
  
  if (blocks <= NDIRECT){
      for (int i=blocks;i<blocks;i++){
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
