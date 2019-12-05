// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&server_mlock,NULL);
  pthread_cond_init(&server_conlock,NULL);
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
	// Your lab2 part2 code goes here
  std::cout<<"lock_server::acquire(beginconfig):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;

  if(clt<0)//get wrong clt
  {
    ret = lock_protocol::RPCERR;
    std::cout<<"lock_server::acquire(error):"<<"clt:"<<clt<<std::endl;
    return ret;
  }
  else
  {
    //lock the server_mutex_lock
    pthread_mutex_lock(&server_mlock);

    if(lock_map.find(lid)!=lock_map.end())//the lockid already exsisted
    {
      while(lock_map[lid].lock_status==true)//the target lock is still on lock
      {
        pthread_cond_wait(&server_conlock,&server_mlock);
      }
      //end the while, the mutex_lock is on lock again
      std::cout<<"lock_server::acquire(acquire):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;
      //the target lock is unlock,now can get the lock
      lock_map[lid].lock_status=true;
      lock_map[lid].lock_owner=clt;
    }
    else//no exsist lock, create and add a new one
    {
      std::cout<<"lock_server::acquire(new):"<<"clt:"<<clt<<"lockconfig:"<<lock_map[lid].lock_status<<std::endl;
      //set new lock config
      lock_config tmp_lock_config;
      tmp_lock_config.lock_status=true;
      tmp_lock_config.lock_owner=clt;
      //insert the new lock into the lock_map
      lock_map[lid]=tmp_lock_config;
    }
    r=lid;
    nacquire+=1;

    //unlock the server_mutex_lock
    pthread_mutex_unlock(&server_mlock);

    return ret;
  }

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  std::cout<<"lock_server::release(begin):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;
      
  if(clt<0)//get wrong clt
  {
    ret = lock_protocol::RPCERR;
    std::cout<<"lock_server::release(error):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;
    return ret;
  }
  else
  {
    //lock the server_mutex_lock
    pthread_mutex_lock(&server_mlock);

    if(lock_map.find(lid)!=lock_map.end())//the lockid already exsisted
    {
      if(lock_map[lid].lock_status!=false)
      {
        std::cout<<"lock_server::release(release):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;
        lock_map[lid].lock_status=false;//set the status to unlock
        nacquire-=1;
      }
      else
      {
        std::cout<<"lock_server::release(retry):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;
        ret = lock_protocol::RETRY;
      }
    }
    else//target lock does not exsist, report error
    {
      std::cout<<"lock_server::release(none):"<<"clt:"<<clt<<"lid:"<<lid<<std::endl;
      ret = lock_protocol::NOENT;
    }

    r=lid;

    //unlock the server_mutex_lock
    pthread_mutex_unlock(&server_mlock);

    pthread_cond_signal(&server_conlock);//send signal to another thread

    return ret;
  }
  return ret;
}