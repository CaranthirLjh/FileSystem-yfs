// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

struct lock_config{
  //lock_protocol::lockid_t lock_Id;//lock id
  bool lock_status;//lock status:True=Onlock False=Unlock
  int lock_owner;//lock owner:the clt which own the lock
};

class lock_server {

 protected:
  int nacquire;

  //map of lock:lockId-lockConfig
  std::map<lock_protocol::lockid_t,lock_config> lock_map;

  pthread_mutex_t server_mlock;//add mutex
  pthread_cond_t server_conlock;//add conditional variable

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 