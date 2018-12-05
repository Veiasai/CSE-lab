// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <unistd.h>

int lock_client_cache::last_port = 0;

static u_int sendtime=5;
enum {unlock, locked, apply, revokee, hold, discard} state;

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
  char hname[100];
  VERIFY(gethostname(hname, sizeof(hname)) == 0);
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
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  while(lock[lid] == discard){
    pthread_cond_wait(&cond[lid], &mutex);
  }

  if (lock[lid] == unlock){
    lock[lid] = apply;
    int tr = 9;
    int ac_ret;
    std::cerr << "applying\n"; 
    pthread_mutex_unlock(&mutex);
    ac_ret = cl->call(lock_protocol::acquire, lid, id, tr);
    pthread_mutex_lock(&mutex);
    if (lock[lid] == revokee){
      std::cerr << "applying revoke\n"; 
    }
    else if (ac_ret == lock_protocol::RETRY){
      std::cerr << "applying retry\n"; 
      pthread_cond_wait(&cond[lid], &mutex);
    }else if(ac_ret == rlock_protocol::REVOKE){
      lock[lid] = revokee;
    }else if (lock[lid] == apply){
      std::cerr << "applying ok\n"; 
      lock[lid] = locked;
    }
  }
  else if (lock[lid] == hold){
    lock[lid] = locked;
  }else{
    pthread_cond_wait(&cond[lid], &mutex);
  }
  pthread_mutex_unlock(&mutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  if (!pthread_cond_destroy(&cond[lid])){
    pthread_cond_init(&cond[lid], NULL);
    if (lock[lid] == revokee){
      lock[lid] = discard;
      pthread_mutex_unlock(&mutex);
      int tr = 9;
      cl->call(lock_protocol::release, lid, id, tr);
      pthread_mutex_lock(&mutex);
      lock[lid] = unlock;
      //std::cerr << "release 3" << '\n';
    }else{
      lock[lid] = hold;
      //std::cerr << "release 4" << '\n';
    }
    //std::cerr << "release 5" << '\n';
  }
  else{
    //std::cerr << "release 6" << '\n';
    pthread_cond_signal(&cond[lid]);
  }
  //std::cerr << "release 7" << '\n';
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int & r)
{
  int ret = rlock_protocol::OK;
  std::cerr << "revoke 1 " << lock[lid] << '\n';
  pthread_mutex_lock(&mutex);
  if (lock[lid] == hold){
    lock[lid] = discard;
    ret = 1;
  }else if (lock[lid] == discard){
    lock[lid] = unlock;
    pthread_cond_broadcast(&cond[lid]);
  }else if (lock[lid] > unlock){
    lock[lid] = revokee;
    std::cerr << "revoke 2" << '\n';
  }
  std::cerr << "revoke done" << '\n';
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,  int state, int & r)
{
  std::cerr << "retry " << state << '\n';
  pthread_mutex_lock(&mutex);
  int ret = rlock_protocol::OK;
  if (lock[lid] == apply){
    if (state) lock[lid] = revokee;
    else lock[lid] = locked;
    pthread_cond_signal(&cond[lid]);
    r = rlock_protocol::OK;
  }else{
    r = 2;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}



