#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <set>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

//the property of lock in server
struct lock_property{
  public:
    //struct function
    lock_property(){};
    ~lock_property(){};
    //properties
    bool state;
    pthread_mutex_t mutex_lock;
    pthread_cond_t cond_lock;
    std::string owner;
    std::set<std::string> wait_list;
    bool revoke;
};


class lock_server_cache {
 private:
  int nacquire;

  //lock of the lock map
  pthread_mutex_t lock_map_mlock;
  //lock map
  std::map<lock_protocol::lockid_t,lock_property> lock_map;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
