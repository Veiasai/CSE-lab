// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

int lock_client_cache::last_port = 0;

static u_int sendtime=5;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  for (int i=0;i<1100;i++){
    cond[i] = PTHREAD_COND_INITIALIZER;
  }
  mutex = PTHREAD_MUTEX_INITIALIZER;
  std::cerr << "client init" << '\n';
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r = 5;
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  while(lock[lid] == 3){
    pthread_cond_wait(&cond[lid], &mutex);
  }
  if (lock[lid] == 0){
    lock[lid] = 2;
    pthread_mutex_unlock(&mutex);
    while(r >= 5) cl->call(lock_protocol::acquire, lid, id, r);
    pthread_mutex_lock(&mutex);
    if (r == lock_protocol::RETRY){
      pthread_cond_wait(&cond[lid], &mutex);
    }else{
      lock[lid] = 1;
    }
  }
  else{
    pthread_cond_wait(&cond[lid], &mutex);
  }
  pthread_mutex_unlock(&mutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = rlock_protocol::OK;
  int r = 9;
  pthread_mutex_lock(&mutex);
  if (!pthread_cond_destroy(&cond[lid])){
    pthread_cond_init(&cond[lid], NULL);
    lock[lid] = 3;
    pthread_mutex_unlock(&mutex);
    while(r >= 5) cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&mutex);
    if (lock[lid] == 3){
      lock[lid] = 0;
      pthread_cond_broadcast(&cond[lid]);
    }
  }else{
    pthread_cond_signal(&cond[lid]);
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int & r)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  if (lock[lid] == 1){
    pthread_cond_wait(&cond[lid], &mutex);
    r = 0;
    if (pthread_cond_destroy(&cond[lid])){
      lock[lid] = 2;
      r = 2;
    }else{
      pthread_cond_init(&cond[lid], NULL);
    }
  }
  else{
    r = 1;
  }
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int & r)
{
  std::cerr << "retry" << '\n';
  //pthread_mutex_lock(&mutex);
  int ret = rlock_protocol::OK;
  if (__sync_bool_compare_and_swap(&lock[lid], 2, 1)){
    pthread_cond_signal(&cond[lid]);
    r = rlock_protocol::OK;
  }else{
    r = 2;
  }
  //pthread_mutex_unlock(&mutex);
  return ret;
}



