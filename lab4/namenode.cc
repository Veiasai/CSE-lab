#include "namenode.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sys/stat.h>
#include <unistd.h>
#include "threader.h"

using namespace std;

#define DB 1

void NameNode::init(const string &extent_dst, const string &lock_dst) {
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  yfs = new yfs_client(extent_dst, lock_dst);

  lc->acquire(1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  lc->release(1);
  /* Add your init logic here */
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
  return list<LocatedBlock>();
}

bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) {
  ec->complete(ino, new_size);
  return false;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
  
  blockid_t bid;
  ec->append_block(ino, bid);
  extent_protocol::attr attr;
  ec->getattr(ino, attr);
  LocatedBlock lb(bid, attr.size, BLOCK_SIZE, master_datanode);
  return lb;
  // throw HdfsException("Not implemented");
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
  string src_buf, dst_buf;
  ec->get(src_dir_ino, src_buf);
  ec->get(dst_dir_ino, dst_buf);
  bool found = false;
  int pos = 0;
  int dst_ino;
  while(pos < src_buf.size()){
      const char * t = src_buf.c_str()+pos;
      if (strcmp(t, src_name.c_str()) == 0){
          found = true;
          dst_ino = *(uint32_t *)(t + strlen(t) + 1);
          src_buf.erase(pos, strlen(t) + 1 + sizeof(uint));
          break;
      }
      pos += strlen(t) + 1 + sizeof(uint);
  }
  if (found){
    dst_buf += dst_name;
    dst_buf.resize(dst_buf.size() + 5, 0);
    *(uint32_t *)(dst_buf.c_str()+dst_buf.size()-4) = dst_ino;
    ec->put(dst_dir_ino, dst_buf);
    ec->put(src_dir_ino, src_buf);
  }
  return found;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  #if DB 
  std::cout << "mkdir:" << name << "mode:" << mode << std::endl;
  #endif
  return !yfs->create(parent, name.c_str(), extent_protocol::T_DIR, ino_out);
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  #if DB 
  std::cout << "create:" << name << "mode:" << mode << std::endl;
  #endif
  return !yfs->create(parent, name.c_str(), extent_protocol::T_FILE, ino_out);
}

bool NameNode::Isfile(yfs_client::inum ino) {
  return yfs->isfile(ino);
}

bool NameNode::Isdir(yfs_client::inum ino) {
  return yfs->isdir(ino);
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &info) {
  return !yfs->getfile(ino, info);
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &info) {
  return !yfs->getdir(ino, info);
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
  return !yfs->readdir(ino, dir);
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
  return !yfs->unlink(parent, name.c_str());
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
  return list<DatanodeIDProto>();
}
