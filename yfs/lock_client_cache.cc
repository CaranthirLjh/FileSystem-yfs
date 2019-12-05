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

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
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

  //init mutex_lock
  pthread_mutex_init(&client_mlock,NULL);
  //init cond_lock
  pthread_cond_init(&client_clock,NULL);

  //init maps
  lock_map.clear();
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  std::cout<<"lock_client_cache::acquire: Lockid:"<<lid<<std::endl;
	int ret = lock_protocol::OK;
  //printf("acquire: Lockid->%lld\n",lid);
	//if target lock doesn't exsist, init a new one
	pthread_mutex_lock(&client_mlock);
	if(lock_map.find(lid)==lock_map.end())
  {
    printf("acquire: New Lockid->%lld\n",lid);

    lock_map[lid].state=NONE;

    pthread_mutex_init(&(lock_map[lid].mutex_lock),NULL);
    pthread_cond_init(&(lock_map[lid].cond_lock),NULL);

    lock_map[lid].revoke=false;
    lock_map[lid].retry=false;
    
  }
	pthread_mutex_unlock(&client_mlock);


  bool retry_state=true;//judge whether have to retry
  pthread_mutex_lock(&(lock_map[lid].mutex_lock));
  while(retry_state==true)
  {
    //lock the mutex lock of the target lock  
    switch(lock_map[lid].state)
    {
      case NONE://the client has not got the lock yet, have to ask the server to get the lock
      {
        //turn lock state to acquiring
        lock_map[lid].state=ACQUIRING;
        //unlock before send RPC
        pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

        //ask server
        int acq_int=0;
        int acq_return=lock_protocol::OK;
        acq_return=cl->call(lock_protocol::acquire,lid,id,acq_int);

        //back to client
        pthread_mutex_lock(&(lock_map[lid].mutex_lock));
        //succeed in getting the lock
        if(acq_return==lock_protocol::OK)
        {
          printf("acquire: (NONE)Success Acq Lockid->%lld\n",lid);

          //set the lock state
          lock_map[lid].state=LOCKED;

          ret=lock_protocol::OK;
          //no need to retry
          retry_state=false;
        }
        //the target lock is not free, plz retry
        else if(acq_return==lock_protocol::RETRY)
        {
          printf("acquire: Start Retry Acq Lockid->%lld\n",lid);
          //wait until the server send a retry signal
          while(lock_map[lid].retry==false)
          {
            pthread_cond_wait(&(lock_map[lid].cond_lock),&(lock_map[lid].mutex_lock));
          }
          printf("acquire: Success Retry Acq Lockid->%lld\n",lid);
          //retry:
          //set state
          lock_map[lid].state=NONE;
          lock_map[lid].retry=false;
          //send signal to retry hander
          pthread_cond_broadcast(&(lock_map[lid].cond_lock));
        }
        break;
      }
      case FREE://the client has got the lock, just set its state
      {
        printf("acquire: (FREE)Success Acq Lockid->%lld\n",lid);
        //set the lock state
        lock_map[lid].state=LOCKED;

        ret=lock_protocol::OK;
        //no need to retry
        retry_state=false;
        break;
      }
      //the lock is busy, can not acquire it now
      case LOCKED:
      case ACQUIRING:
      case RELEASING:
      {
        printf("acquire: Wait Lockid->%lld\n",lid);
        //wait until signal
        pthread_cond_wait(&(lock_map[lid].cond_lock),&(lock_map[lid].mutex_lock));
        break;
      }
    }
    
  }
  pthread_mutex_unlock(&(lock_map[lid].mutex_lock));
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  std::cout<<"lock_client_cache::release: Lockid:"<<lid<<std::endl;
  //printf("release: Lockid->%lld\n",lid);
  //whether the target lock exsist, if not error:
  //acquire the lock map lock
  pthread_mutex_lock(&client_mlock);
  //if no target lock, error
  if(lock_map.find(lid)==lock_map.end())
  {
    printf("release: Error Lockid->%lld\n",lid);
    // std::cout<<"lock_client_cache::release: No target lock!"<<std::endl;
    VERIFY(0);
  }
  pthread_mutex_unlock(&client_mlock);

  pthread_mutex_lock(&(lock_map[lid].mutex_lock));
  switch(lock_map[lid].state)
  {
    //wrong state
    case NONE:
    case FREE:
    case ACQUIRING:
    case RELEASING:
    {
      printf("release: Wrong State Lockid->%lld\n",lid);
      // std::cout<<"lock_client_cache::release: The lock is in wrong state: "<<lock_map[lid].state<<std::endl;
      VERIFY(0);
    }
    //free the lock
    case LOCKED:
    {
      //case 1:no revoke from the server, just change the state
      if(lock_map[lid].revoke==false)
      {
        printf("release: (N_Revoke)Succsee Fre Lockid->%lld\n",lid);
        lock_map[lid].state=FREE;
      }
      //case 2:has revoke from the server
      else
      {
        lock_map[lid].state=RELEASING;
        lock_map[lid].revoke=false;
        //unlock before send RPC
        pthread_mutex_unlock(&(lock_map[lid].mutex_lock));
        //call server's release function 
        int rel_int=0;
        int rel_result=lock_protocol::OK;
        rel_result=cl->call(lock_protocol::release,lid,id,rel_int);

        pthread_mutex_lock(&(lock_map[lid].mutex_lock));
        if(rel_result==lock_protocol::OK)
        {
          printf("release: (Revoke)Succsee Fre Lockid->%lld\n",lid);
          lock_map[lid].state=NONE;
        }
        else
        {
          std::cout<<"lock_client_cache::release: The return of server is wrong"<<std::endl;
          VERIFY(0);
        }
      }
      //send signal to other client
      pthread_cond_broadcast(&(lock_map[lid].cond_lock));
    }
  }
  pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  printf("revoke: Lockid->%lld\n",lid);
  int ret = rlock_protocol::OK;
  //whether the target lock exsist, if not error:
  //acquire the lock map lock
  pthread_mutex_lock(&client_mlock);
  //if no target lock, error
  if(lock_map.find(lid)==lock_map.end())
  {
    printf("revoke: Error Lockid->%lld\n",lid);
    // std::cout<<"lock_client_cache::revoke_handler: No target lock!"<<std::endl;
    VERIFY(0);
  }
  pthread_mutex_unlock(&client_mlock);

  pthread_mutex_lock(&(lock_map[lid].mutex_lock));
  switch(lock_map[lid].state)
  {
    // case NONE://the lock has already been revoked
    // {
    //   printf("revoke: (State=None)Already Rev Lockid->%lld\n",lid);
    //   lock_map[lid].revoke=false;
    //   break;
    // }
    case FREE://the target lock is free, call server's revoke function
    {
      //set lock state
      lock_map[lid].state=RELEASING;
      // lock_map[lid].revoke=false;

      pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

      int rel_int=0;
      int rel_result=lock_protocol::OK;
      rel_result=cl->call(lock_protocol::release,lid,id,rel_result);

      pthread_mutex_lock(&(lock_map[lid].mutex_lock));
      if(rel_result==lock_protocol::OK)
      {
        printf("revoke: (FREE)Success Rev Lockid->%lld\n",lid);
        lock_map[lid].state=NONE;
      }
      else
      {
        std::cout<<"lock_client_cache::revoke_handler: The return of server is wrong"<<std::endl;
        VERIFY(0);
      }
      break;
    }
    //the target lock is busy, turn the revoke state to true
    case NONE:
    case ACQUIRING:
    case RELEASING:
    case LOCKED:
    {
      printf("revoke: (N_FREE)Wait Rev Lockid->%lld\n",lid);
      lock_map[lid].revoke=true;
      break;
    }
  }
  //send signal to other client
  pthread_cond_broadcast(&(lock_map[lid].cond_lock));
  pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  printf("retry: Lockid->%lld\n",lid);
  int ret = rlock_protocol::OK;
  //whether the target lock exsist, if not error:
  //acquire the lock map lock
  pthread_mutex_lock(&client_mlock);
  //if no target lock, error
  if(lock_map.find(lid)==lock_map.end())
  {
    printf("retry: Error Lockid->%lld\n",lid);
    std::cout<<"lock_client_cache::retry_handler: No target lock!"<<std::endl;
    VERIFY(0);
  }
  pthread_mutex_unlock(&client_mlock);

  pthread_mutex_lock(&(lock_map[lid].mutex_lock));
  if(lock_map[lid].retry==false)
  {
    //set the target lock's retry state
    lock_map[lid].retry=true;
    //send signal to other client
    pthread_cond_broadcast(&(lock_map[lid].cond_lock));
    printf("retry: Success ReTry Lockid->%lld\n",lid);
  }
  else
  {
    printf("retry: Already ReTry Lockid->%lld\n",lid);
  }
  
  pthread_mutex_unlock(&(lock_map[lid].mutex_lock));

  return ret;
}



