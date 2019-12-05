// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  //init mutex_lock
  pthread_mutex_init(&lock_map_mlock,NULL);

  //init maps
  lock_map.clear();
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  printf("acquire: Lockid->%lld\n",lid);
  lock_protocol::status ret = lock_protocol::OK;

  //whether the target lock exsist, if not init a new lock:
  //acquire the lock map lock
  pthread_mutex_lock(&lock_map_mlock);
  //if no target lock, init a new lock
  if(lock_map.find(lid)==lock_map.end())
  {
    printf("acquire: New Lockid->%lld\n",lid);
    lock_property tmp_lock;

    tmp_lock.state=false;
    pthread_mutex_init(&(tmp_lock.mutex_lock),NULL);
    pthread_cond_init(&(tmp_lock.cond_lock),NULL);
    tmp_lock.owner="";
    tmp_lock.wait_list;
    tmp_lock.revoke=false;

    lock_map[lid]=tmp_lock;
  }
  //add acquire num
  nacquire+=1;
  pthread_mutex_unlock(&lock_map_mlock);

  //acquire the mutex lock of target lock
  pthread_mutex_lock(&(lock_map[lid].mutex_lock));
  //case 1:the target lock is free
  if(lock_map[lid].state==false)
  {
    printf("acquire: Success acq Free Lockid->%lld\n",lid);

    lock_map[lid].state=true;
    //delete from wait list&set owner
    (lock_map[lid].wait_list).erase(id);
    lock_map[lid].owner=id;
    //return the revoke state to false 
    lock_map[lid].revoke=false;
  }
  //case 2:the target lock is not free
  else
  {
    //insert the client id to the wait list
    (lock_map[lid].wait_list).insert(id);
    //case 2-1:the revoke state is true
    if(lock_map[lid].revoke==true)
    {
      printf("acquire: (Already revoke)Wait acq Busy Lockid->%lld\n",lid);
      //turn the result to retry
      ret=lock_protocol::RETRY;
    }
    //case 2-2:the revoke state is false
    else
    {
      //turn the revoke state to true
      lock_map[lid].revoke=true;
      //unlock before send RPC
      pthread_mutex_unlock(&(lock_map[lid].mutex_lock));
      //get the owner of the target lock
      std::string dst=lock_map[lid].owner;
      
      handle h(dst);
      int revoke_int=0;//the second parameter of revoke
      int revoke_ret=lock_protocol::OK;//the return of revoke

      rpcc *cl=h.safebind();
      if (cl) {
        revoke_ret = cl->call(rlock_protocol::revoke,lid,revoke_int);
      }
      //deal error with revoke
      if (!cl || revoke_ret != lock_protocol::OK) {
        // handle failure
        VERIFY(0);
      }
      printf("acquire: (Success revoke)Wait acq Busy Lockid->%lld\n",lid);
      ret=lock_protocol::RETRY;
      return ret;
    }
  }

  pthread_mutex_unlock(&(lock_map[lid].mutex_lock));
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  printf("release: Lockid->%lld\n",lid);

  lock_protocol::status ret = lock_protocol::OK;
  //whether the target lock exsist:
  //acquire the lock map lock
  pthread_mutex_lock(&lock_map_mlock);
  //if no target lock, error
  if(lock_map.find(lid)==lock_map.end())
  {
    std::cout<<"lock_server_cache::release: no target lock!"<<std::endl;
    VERIFY(0);
  }
  pthread_mutex_unlock(&lock_map_mlock);

  //acquire the mutex lock of target lock
  pthread_mutex_lock(&(lock_map[lid].mutex_lock));
  //turn the lock state to false & empty the lock owner & turn the revoke state to false
  lock_map[lid].state=false;
  lock_map[lid].owner="";
  lock_map[lid].revoke=false;
  //case 1:the wait set is not empty
  if((lock_map[lid].wait_list).empty()==false)
  {
    printf("release: Still Wait Member Lockid->%lld\n",lid);
    //unlock before send RPC
    // pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

    handle h(*((lock_map[lid].wait_list).begin()));
    int retry_int=0;//the second parameter of retry
    int retry_ret=lock_protocol::OK;//the return of retry

    rpcc *cl=h.safebind();
    if (cl) {
      retry_ret = cl->call(rlock_protocol::retry,lid,retry_int);
    }
    //deal error with retry
    if (!cl || retry_ret != lock_protocol::OK) {
      // handle failure
      std::cout<<"lock_server_cache::release: Retry error!"<<std::endl;
      VERIFY(0);
    }

    //if wait size member > 1, send revoke signal
    if((lock_map[lid].wait_list).size()>1)
    {
      printf("release: More Than 1 Wait Member Lockid->%lld\n",lid);
      int revoke_int=0;//the second parameter of revoke
      int revoke_ret=lock_protocol::OK;//the return of revoke

      revoke_ret=cl->call(rlock_protocol::revoke,lid,revoke_int);

      ret=revoke_ret;
    }

    // return ret;
  }
  pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

