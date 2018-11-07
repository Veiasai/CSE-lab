// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache():
  nacquire (0)
{
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::cout <<  lid << " "<< id <<" "<< "a\n";
  pthread_mutex_lock(&mutex);
  if (lock[lid].empty()){
    lock[lid].push(id);
    r = lock_protocol::OK;
    std::cout <<  lid << " "<< id <<" "<< "a1\n";
  }else if (lock[lid].front() != id){
    std::cout <<  lid << " "<< id <<" "<< "a2\n";
    if (find(wait_set[lid].begin(), wait_set[lid].end(), id) == wait_set[lid].end()){
      sockaddr_in dstsock;
      make_sockaddr(lock[lid].back().c_str(), &dstsock);
      rpcc *cl = new rpcc(dstsock);
      if (cl->bind() < 0) {
        printf("lock_client: call bind\n");
      }
      lock[lid].push(id);
      wait_set[lid].insert(id);
      int tr = 9;
      pthread_mutex_unlock(&mutex);
      while (tr == 9) cl->call(rlock_protocol::revoke, lid, tr);
      delete cl;
      return ret;
    }
    r = lock_protocol::RETRY;
  }else{
    r = lock_protocol::OK;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::cout << lid <<" "<<  id << " "<< "r\n";
  pthread_mutex_lock(&mutex);
  if (!lock[lid].empty() && lock[lid].front() == id){
    lock[lid].pop();
    wait_set[lid].erase(id);
  }
  else{
    pthread_mutex_unlock(&mutex);
    return ret;
  }
  //std::cout << "next\n";
  if (!lock[lid].empty()){
    std::string t = lock[lid].front();
    sockaddr_in dstsock;
    make_sockaddr(t.c_str(), &dstsock);
    rpcc *cl = new rpcc(dstsock);
    if (cl->bind() < 0) {
      printf("lock_client: call bind\n");
    }
    //std::cout << "next2\n";
    int tr = 9;
    pthread_mutex_unlock(&mutex);
    while(tr == 9) cl->call(rlock_protocol::retry, lid, tr);
    delete cl;
    return ret;
  }
  //std::cout << "next3\n";
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

