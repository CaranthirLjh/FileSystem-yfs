#include "datanode.h"
#include <arpa/inet.h>
#include "extent_client.h"
#include <unistd.h>
#include <algorithm>
#include "threader.h"

using namespace std;

int DataNode::init(const string &extent_dst, const string &namenode, const struct sockaddr_in *bindaddr) {
  cout<<"DataNode::init data node:"<<namenode<<endl;
  ec = new extent_client(extent_dst);

  // Generate ID based on listen address
  id.set_ipaddr(inet_ntoa(bindaddr->sin_addr));
  id.set_hostname(GetHostname());
  id.set_datanodeuuid(GenerateUUID());
  id.set_xferport(ntohs(bindaddr->sin_port));
  id.set_infoport(0);
  id.set_ipcport(0);

  // Save namenode address and connect
  make_sockaddr(namenode.c_str(), &namenode_addr);
  if (!ConnectToNN()) {
    delete ec;
    ec = NULL;
    return -1;
  }

  // Register on namenode
  if (!RegisterOnNamenode()) {
    delete ec;
    ec = NULL;
    close(namenode_conn);
    namenode_conn = -1;
    return -1;
  }

  /* Add your initialization here */
  NewThread(this,&DataNode::Heartbeat);

  if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir

  return 0;
}

void DataNode::Heartbeat()
{
  while(true)
  {
    SendHeartbeat();
    sleep(1);
  }
}

bool DataNode::ReadBlock(blockid_t bid, uint64_t offset, uint64_t len, string &buf) {
  cout<<"DataNode::ReadBlock: Start! bid:"<<bid<<endl;
  /* Your lab4 part 2 code */
  //get all content of the target block
  string block_buf="";
  int r_readblock = ec->read_block(bid,block_buf);
  // if(r_readblock!=extent_protocol::OK)
  // {
  //   cout<<"DataNode::ReadBlock: Read block fail."<<endl;
  //   return false;
  // }
  //get the target buf
  if(offset>block_buf.size())
  {
    buf="";
  }
  else
  {
    buf=block_buf.substr(offset,len);
  }
  cout<<"DataNode::ReadBlock: offset:"<<offset<<",len:"<<len<<",buf size:"<<buf.size()<<endl;
  
  return true;
}

bool DataNode::WriteBlock(blockid_t bid, uint64_t offset, uint64_t len, const string &buf) {
  cout<<"DataNode::WriteBlock: Start! bid:"<<bid<<",offset:"<<offset<<",len:"<<len<<",buf size:"<<buf.size()<<endl;
  /* Your lab4 part 2 code */
  //get all content of the target block
  string block_buf="";
  int r_readblock = ec->read_block(bid,block_buf);
  // if(r_readblock!=extent_protocol::OK)
  // {
  //   cout<<"DataNode::WriteBlock: Read block fail."<<endl;
  //   return false;
  // }
  //write the target buf
  string read_content="";
  ec->read_block(bid,read_content);
  string write_content= read_content.substr(0,offset)+buf+read_content.substr(offset+len);
  ec->write_block(bid,write_content);

  return true;
}

