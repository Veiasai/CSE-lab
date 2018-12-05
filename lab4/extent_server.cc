// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <rpc.h>

extent_server::extent_server() 
{
  im = new inode_manager();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);

  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int & r)
{
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  im->getattr(id, a);

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
 
  return extent_protocol::OK;
}

int extent_server::append_block(extent_protocol::extentid_t id, blockid_t &bid)
{
  id &= 0x7fffffff;

  im->append_block(id, bid);

  return extent_protocol::OK;
}

int extent_server::get_block_ids(extent_protocol::extentid_t id, std::list<blockid_t> &block_ids)
{
  id &= 0x7fffffff;

  im->get_block_ids(id, block_ids);

  return extent_protocol::OK;
}

int extent_server::read_block(blockid_t id, std::string &buf)
{
  char _buf[BLOCK_SIZE];

  im->read_block(id, _buf);
  buf.assign(_buf, BLOCK_SIZE);

  return extent_protocol::OK;
}

int extent_server::write_block(blockid_t id, std::string buf, int &)
{
  if (buf.size() != BLOCK_SIZE)
    return extent_protocol::IOERR;

  im->write_block(id, (const char *) buf.data());

  return extent_protocol::OK;
}

int extent_server::complete(extent_protocol::extentid_t eid, uint32_t size, int &)
{
  im->complete(eid, size);
  return extent_protocol::OK;
}

