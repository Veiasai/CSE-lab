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
  yfs = new yfs_client(ec, lc);

  /* Add your init logic here */
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
  #if DB 
  std::cout << "get locations:" << ino << std::endl;
  cout.flush();
  #endif

  list<NameNode::LocatedBlock> l;
  long long size = 0;
  list<blockid_t> block_ids;
  ec->get_block_ids(ino, block_ids);
  extent_protocol::attr attr;
  ec->getattr(ino, attr);
  #if DB 
    std::cout << "get locations attr size:" << attr.size << std::endl;
    cout.flush();
  #endif
  int i = 0;
  for(auto item : block_ids){
    #if DB 
    std::cout << "get locations block id:" << item << std::endl;
    cout.flush();
    #endif
    i++;
    LocatedBlock lb(item, size, i < block_ids.size() ? BLOCK_SIZE : (attr.size - size), master_datanode);
    l.push_back(lb);
    size += BLOCK_SIZE;
  }
  return l;
}

bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) {
  #if DB 
  std::cout << "complete:" << ino << "size:" << new_size << std::endl;
  cout.flush();
  #endif

  bool res = !ec->complete(ino, new_size);
  if (res)
    lc->release(ino);
  return res;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
  #if DB 
  std::cout << "append:" << ino << std::endl;
  cout.flush();
  #endif

  blockid_t bid;
  extent_protocol::attr attr;
  ec->getattr(ino, attr);
  ec->append_block(ino, bid);
  // pendingWrite[ino] += BLOCK_SIZE;
  LocatedBlock lb(bid, attr.size, (attr.size % BLOCK_SIZE) ? attr.size % BLOCK_SIZE : BLOCK_SIZE, master_datanode);
  return lb;
  // throw HdfsException("Not implemented");
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
  #if DB 
  std::cout << "rename:" << src_dir_ino << src_name << " dst:" << dst_dir_ino << dst_name << std::endl;
  cout.flush();
  #endif

  string src_buf, dst_buf;
  ec->get(src_dir_ino, src_buf);
  ec->get(dst_dir_ino, dst_buf);
  bool found = false;
  int pos = 0;
  int dst_ino;
  const char * t;
  while(pos < src_buf.size()){
      t = src_buf.c_str()+pos;
      if (strcmp(t, src_name.c_str()) == 0){
          found = true;
          dst_ino = *(uint32_t *)(t + strlen(t) + 1);
          src_buf.erase(pos, strlen(t) + 1 + sizeof(uint));
          break;
      }
      pos += strlen(t) + 1 + sizeof(uint);
  }
  if (found){
    if (src_dir_ino == dst_dir_ino){
      dst_buf = src_buf;
    }
    dst_buf += dst_name;
    dst_buf.resize(dst_buf.size() + 5, 0);
    *(uint32_t *)(dst_buf.c_str()+dst_buf.size()-4) = dst_ino;
    ec->put(src_dir_ino, src_buf);
    ec->put(dst_dir_ino, dst_buf);
    // pendingWrite.insert(make_pair(src_dir_ino, src_buf.size()));
    // pendingWrite.insert(make_pair(dst_dir_ino, dst_buf.size()));
  }
  return found;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  #if DB 
  std::cout << "mkdir:" << name << "mode:" << mode << std::endl;
  cout.flush();
  #endif

  bool res = !yfs->mkdir(parent, name.c_str(), mode, ino_out);
  // if (res){
  //   pendingWrite.insert(make_pair(ino_out, 0));
  // }
  return res;
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  #if DB 
  std::cout << "create:" << name << "mode:" << mode << std::endl;
  cout.flush();
  #endif

  bool res =  !yfs->create(parent, name.c_str(), mode, ino_out);
  if (res) {
    lc->acquire(ino_out);
    pendingWrite.insert(make_pair(ino_out, 0));
  }
  return res;
}

bool NameNode::Isfile(yfs_client::inum inum) {
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
      printf("error getting attr\n");
      return false;
  }

  if (a.type == extent_protocol::T_FILE) {
      printf("isfile: %lld is a file\n", inum);
      return true;
  }else if (a.type == extent_protocol::T_SYMLK) {
      printf("isfile: %lld is a symlink\n", inum);
      return false;
  } 
  return false;
}

bool NameNode::Isdir(yfs_client::inum inum) {
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
      printf("error getting attr\n");
      return false;
  }
  if (a.type == extent_protocol::T_DIR) {
      printf("isfile: %lld is a dir\n", inum);
      return true;
  } 
  printf("isfile: %lld is not a dir\n", inum);
  return false;
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &fin) {
  extent_protocol::attr a;
  if (ec->getattr(ino, a) != extent_protocol::OK) {
      return false;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;

  return true;
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &din) {
    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        return false;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
    return true;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
  std::string buf;
  ec->get(ino, buf);
  int pos = 0;
  while(pos < buf.size()){
      struct yfs_client::dirent temp;
      temp.name = std::string(buf.c_str()+pos);
      temp.inum = *(uint32_t *)(buf.c_str() + pos + temp.name.size() + 1);
      dir.push_back(temp);
      pos += temp.name.size() + 1 + sizeof(uint32_t);
  }
  return true;
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
  std::string buf;
  ec->get(parent, buf);

  int pos = 0;
  while(pos < buf.size()){
      const char * t = buf.c_str()+pos;
      if (strcmp(t, name.c_str()) == 0){
          uint32_t ino = *(uint32_t *)(t + strlen(t) + 1);
          buf.erase(pos, strlen(t) + 1 + sizeof(uint32_t));
          ec->put(parent, buf);
          ec->remove(ino);
          goto done;
      }
      pos += strlen(t) + 1 + sizeof(uint32_t);
  }
  return false;
  done:
  return true;
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
  list<DatanodeIDProto> l;
  l.push_back(master_datanode);
  return l;
}
