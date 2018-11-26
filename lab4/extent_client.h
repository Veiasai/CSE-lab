// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
 private:
  rpcc *cl;

 public:
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status get_block_ids(extent_protocol::extentid_t eid, std::list<blockid_t> &block_ids);
  extent_protocol::status read_block(blockid_t bid, std::string &buf);
  extent_protocol::status write_block(blockid_t bid, const std::string &buf);
  extent_protocol::status append_block(extent_protocol::extentid_t eid, blockid_t &bid);
  extent_protocol::status complete(extent_protocol::extentid_t eid, uint32_t size);
};

#endif 

