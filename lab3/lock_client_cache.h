// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

//client state
enum client_state {
  NONE,
  FREE,
  LOCKED,
  ACQUIRING,
  RELEASING
};

//the property of lock in client
struct lock_property{
  public:
    //struct function
    lock_property(){};
    ~lock_property(){};
    //properties
    client_state state;
    pthread_mutex_t mutex_lock;
    pthread_cond_t cond_lock;
    bool revoke;
    bool retry;
};

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

  

  //client lock
  pthread_mutex_t client_mlock;
  pthread_cond_t client_clock;

  //lock map
  std::map<lock_protocol::lockid_t,lock_property> lock_map;
  
 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
