#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include <set>
#include <queue>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <algorithm>


class lock_server_cache {
 private:
  int nacquire;
  std::map<lock_protocol::lockid_t, std::queue<std::string> > lock;
  std::map<lock_protocol::lockid_t, std::set<std::string> > wait_set;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
