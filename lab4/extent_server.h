// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <list>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;

 public:
  extent_server();

  int create(uint32_t type, extent_protocol::extentid_t &id);
  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
  int get_block_ids(extent_protocol::extentid_t id, std::list<blockid_t> &);
  int read_block(blockid_t id, std::string &buf);
  int write_block(blockid_t id, std::string buf, int &);
  int append_block(extent_protocol::extentid_t eid, blockid_t &bid);
  int complete(extent_protocol::extentid_t eid, uint32_t size, int &);
};

#endif 







