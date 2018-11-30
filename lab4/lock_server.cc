// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_mutex_t lock_server::mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lock_server::cond = PTHREAD_COND_INITIALIZER;

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  
  while(lock[lid] == 1)
    pthread_cond_wait(&cond, &mutex);
  
  r=lock_protocol::OK;
  lock[lid] = 1;
	// Your lab2 part2 code goes here
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  pthread_mutex_lock(&mutex);
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  if (lock[lid] == 0)
    r=lock_protocol::NOENT;
  else{
    lock[lid] = 0;
    r=lock_protocol::OK;
    pthread_cond_signal(&cond);
  }
    
	// Your lab2 part2 code goes here
  pthread_mutex_unlock(&mutex);
  return ret;
}
