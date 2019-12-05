#include "namenode.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sys/stat.h>
#include <unistd.h>
#include "threader.h"

using namespace std;

string NameNode::inum2str(yfs_client::inum inum)
{
  ostringstream ost;
  ost << inum;
  return ost.str();
}

void NameNode::init(const string &extent_dst, const string &lock_dst) {
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  yfs = new yfs_client(ec, lc);

  /* Add your init logic here */
  heartbeat_count=0;
  NewThread(this,&NameNode::SendHeartbeat);
  
}

void NameNode::SendHeartbeat()
{
  while(true)
  {
    heartbeat_count++;
    sleep(1);
  }
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
  cout<<"NameNode::GetBlockLocations: Start!"<<endl;
  //call get_block_ids to get list of blockid
  std::list<blockid_t> bid_list;
  ec->get_block_ids(ino,bid_list);
  
  //get the ino's attr 
  extent_protocol::attr ino_attr;
  ec->getattr(ino,ino_attr);

  //convert block_id to LocatedBlock
  list<LocatedBlock> LB_list;
  std::list<blockid_t>::iterator iter;
  
  //counter of the block num:
  int block_index=0;
  //offset:
  unsigned long long offset=0;
  for(iter=bid_list.begin();iter!=bid_list.end();iter++)
  {
    unsigned long long size=0;
    //case1: the tmp block is not the last one
    if(block_index<bid_list.size()-1)
    {
      size = BLOCK_SIZE;
    }
    //case1: the tmp block is the last one
    else
    {
      size = ino_attr.size-offset;
    }
    LocatedBlock tmpLB(*iter,offset,size,GetDatanodes());
    LB_list.push_back(tmpLB);
    //update offset
    offset+=BLOCK_SIZE;
    //update block_index
    block_index+=1;
  }

  return LB_list;
}

bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) {
  cout<<"NameNode::Complete: Start!"<<endl;
  //call complete
  int r_complete=ec->complete(ino,new_size);

  if(r_complete!=extent_protocol::OK)
  {
    //unlock the file
    cout<<"NameNode::Complete: Fail!"<<endl;
    //lc->release(ino);
    return false;
  }
  cout<<"NameNode::Complete: Success!"<<endl;
  lc->release(ino);
  return true;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
  cout<<"NameNode::AppendBlock: Start!"<<endl;
  // throw HdfsException("Not implemented");
  blockid_t bid=0;
  //get attr
  extent_protocol::attr attr;
  ec->getattr(ino,attr);
  //call append_block
  ec->append_block(ino,bid);

  //calculate size
  unsigned long long size=0;
  if(attr.size%BLOCK_SIZE==0)
  {
    size=BLOCK_SIZE;
  }
  else
  {
    size=attr.size%BLOCK_SIZE;
  }
  //add to HDFS block
  HDFS_block.insert(bid);
  //convert to LB
  LocatedBlock reLB(bid,attr.size,size,GetDatanodes());

  return reLB;
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
  cout<<"NameNode::Rename: Start!"<<endl;
  cout<<"NameNode::Rename: src_dir_ino:"<<src_dir_ino<<"src_name:"<<src_name<<endl;
  cout<<"NameNode::Rename: dst_dir_ino:"<<dst_dir_ino<<"dst_name:"<<dst_name<<endl;
  bool r = true;
  
  //find the target entry
  bool src_found=false;
  yfs_client::inum target_inum=0;
  yfs->nlock_lookup(src_dir_ino,src_name.c_str(),src_found,target_inum);
  //if src entry doesn't exsist
  if(src_found==false)
  {
    cout<<"NameNode::Rename: src entry doesn't exsist."<<endl;
    r = false;
  }
  else
  {
    bool find_entry=false;
    //read the src_dir
    list<yfs_client::dirent> src_entry_list;
    int r_readdir = yfs->nlock_readdir(src_dir_ino,src_entry_list);

    string src_entry = "";
    int r_getdir = ec->get(src_dir_ino,src_entry);
    //delete the target entry from the src & record all deleted entries
    list<yfs_client::dirent> delete_entry;
    list<yfs_client::dirent>::iterator iter = src_entry_list.begin();
    //delete from src entry
    for(;iter!=src_entry_list.end();)
    {
      //find target entry
      if(iter->name==src_name)
      {
        //change the name
        iter->name=dst_name;
        //add to delete entry
        delete_entry.push_back(*iter);
        //delete from src entry list
        src_entry_list.erase(iter);

        find_entry = true;

        break;
      }
      else
      {
        iter++;
      }
    }

    r = find_entry;

    //write the src result back to src dir
    if(find_entry)
    {
      unsigned int src_begin = src_entry.find(src_name);
      unsigned int first_dot = 0;
      unsigned int second_dot = 0;
      if(src_begin != string::npos)
      {
        first_dot = src_entry.find(";",src_begin);
        if(first_dot != string::npos)
        {
          second_dot = src_entry.find(";",first_dot+1);
          if(second_dot != string::npos)
          {
            if(src_begin!=0)
            {
              src_entry.erase(src_begin-1,(second_dot-src_begin));
            }
            else
            {
              src_entry.erase(src_begin,(second_dot-src_begin));
            }
          }
          else
          {
            if(src_begin!=0)
            {
              src_entry.erase(src_begin-1,(src_entry.size()-src_begin+1));
            }
            else
            {
              src_entry.erase(src_begin,(src_entry.size()-src_begin));
            }
          }
        }
      }
      // string dir_list="";
      // for(iter=src_entry_list.begin();iter!=src_entry_list.end();iter++)
      // {
      //   if(dir_list.size()==0)
      //   {
      //       dir_list=iter->name+","+inum2str(iter->inum);
      //   }
      //   else
      //   {
      //       dir_list=dir_list+","+iter->name+","+inum2str(iter->inum);
      //   }
      // }
      ec->put(src_dir_ino,src_entry);
      cout<<"NameNode::Rename: New src_entry:"<<src_entry<<endl;
      //get the src entry
      string dst_entry="";
      if(src_dir_ino==dst_dir_ino)
      {
        dst_entry=src_entry;
      }
      else
      {
        if(ec->get(dst_dir_ino,dst_entry)!=extent_protocol::OK)
        {
          cout<<"NameNode::Rename: get src dir entry fail."<<endl;
          r = false;
        }
      }
      //insert the new dir item
      for(iter=delete_entry.begin();iter!=delete_entry.end();iter++)
      {
        if(dst_entry.size()==0)
        {
            dst_entry=iter->name+";"+inum2str(iter->inum);
        }
        else
        {
            dst_entry=dst_entry+";"+iter->name+";"+inum2str(iter->inum);
        }
      }
      cout<<"NameNode::Rename: New dst_entry:"<<dst_entry<<endl;
      //write the result entry to the dst dir
      ec->put(dst_dir_ino,dst_entry);
    }
    
  }
  
  return r;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  cout<<"NameNode::Mkdir: Start!"<<endl;
  int res_mkdir = yfs->mkdir(parent,name.c_str(),mode,ino_out);
  if(res_mkdir == yfs_client::OK)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  cout<<"NameNode::Create: Start!"<<endl;
  int res_create = yfs->create(parent,name.c_str(),mode,ino_out);
  if(res_create!=yfs_client::OK)
  {
    cout<<"NameNode::Create: create file fail"<<endl;
    return false;
  }
  else
  {
    //acquire the lock of created file
    cout<<"NameNode::Create: create file success! ino_out:"<<ino_out<<endl;
    lc->acquire(ino_out);
    return true;
  }
}

bool NameNode::Isfile(yfs_client::inum ino) {
  cout<<"NameNode::Isfile: Start! inum:"<<ino<<endl;
  bool b_isfile=yfs->nlock_isfile(ino);
  cout<<"NameNode::Isfile: bool_result:"<<b_isfile<<endl;
  return b_isfile;
}

bool NameNode::Isdir(yfs_client::inum ino) {
  cout<<"NameNode::Isdir: Start! inum:"<<ino<<endl;
  bool b_isdir=yfs->nlock_isdir(ino);
  cout<<"NameNode::Isdir: Success! inum:"<<ino<<endl;
  return b_isdir;
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &info) {
  cout<<"NameNode::Getfile: Start!"<<endl; 
    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        goto release;
    }

    info.atime = a.atime;
    info.mtime = a.mtime;
    info.ctime = a.ctime;
    info.size = a.size;
  cout<<"NameNode::Getfile: Success!"<<endl; 
    return true;

release:
  cout<<"NameNode::Getfile: Fail!"<<endl; 
    return false;
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &info) {
  cout<<"NameNode::Getdir: Start!"<<endl; 
    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        goto release;
    }
    info.atime = a.atime;
    info.mtime = a.mtime;
    info.ctime = a.ctime;
  cout<<"NameNode::Getdir: Success!"<<endl; 
    return true;

release:
  cout<<"NameNode::Getdir: Fail!"<<endl; 
    return false;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
  cout<<"NameNode::Readdir: Start!"<<endl; 
  int res_readdir = yfs->nlock_readdir(ino,dir);
  if(res_readdir == yfs_client::OK)
  {
    cout<<"NameNode::Readdir: Success!"<<endl; 
    return true;
  }
  else
  {
    cout<<"NameNode::Readdir: Fail!"<<endl; 
    return false;
  }
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
  cout<<"NameNode::Unlink: Start!"<<endl; 
  int res_unlink = yfs->nlock_unlink(parent,name.c_str());
  if(res_unlink == yfs_client::OK)
  {
    cout<<"NameNode::Unlink: Success!"<<endl; 
    return true;
  }
  else
  {
    cout<<"NameNode::Unlink: Fail!"<<endl; 
    return false;
  }
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
  // int max_heartbeat = 0;
  // for (auto i : datanode){
  //   max_heartbeat = max(max_heartbeat, i.second);
  // }
  datanode[id] = heartbeat_count;
}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
  cout<<"NameNode::RegisterDatanode: Recovery!"<<endl;
  if(heartbeat_count>5)
  {
    std::set<blockid_t>::iterator iter;
    for(iter=HDFS_block.begin();iter!=HDFS_block.end();iter++)
    {
      ReplicateBlock(*iter,master_datanode,id);
    }
  }
  datanode.insert(make_pair(id,heartbeat_count));
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
  list<DatanodeIDProto> Datanode_list;
  std::map<DatanodeIDProto, int>::iterator iter;
  for(iter=datanode.begin();iter!=datanode.end();iter++)
  {
    if(iter->second>=(heartbeat_count-3))
    {
      Datanode_list.push_back(iter->first);
    }
  }
  return Datanode_list;
}

