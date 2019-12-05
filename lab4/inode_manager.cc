#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  struct superblock *sbPointer = &sb;
  //first data block
  blockid_t fir_data_blockid = IBLOCK(sbPointer->ninodes, sbPointer->nblocks) + 1;
  //last block bit map block
  blockid_t fin_bitmap_blockid = BBLOCK(sbPointer->nblocks);
  //first block bit map block
  blockid_t fir_bitmap_blockid = BBLOCK(fir_data_blockid);
  std::cout<<"block_manager::alloc_block: fir_data_blockid:"<<fir_data_blockid<<std::endl;
  std::cout<<"block_manager::alloc_block: fir_bitmap_blockid:"<<fir_bitmap_blockid<<std::endl;
  std::cout<<"block_manager::alloc_block: fin_bitmap_blockid:"<<fin_bitmap_blockid<<std::endl;
  //alloc a space to store the bitmap in one block
  char *per_block_bitmap = (char *)malloc(BLOCK_SIZE);
  //the first data block bit in bit map's one block
  uint32_t block_bitmap_index = fir_data_blockid % (BLOCK_SIZE * 8);
  for (blockid_t block_index = fir_bitmap_blockid; block_index <= fin_bitmap_blockid; block_index++)
  {
    d->read_block(block_index, per_block_bitmap);
    //judge whether block is free by bit in bitmap
    for (; block_bitmap_index < (BLOCK_SIZE * 8); block_bitmap_index++)
    {
      //index in char
      uint32_t bytePos = block_bitmap_index / 8;
      //bit in byte
      uint32_t bitPos = block_bitmap_index - (8 * bytePos);
      //get byte contain bit of the block
      char bufByte = per_block_bitmap[bytePos];
      //get the target bit
      bool is_block_free = 0;
      is_block_free = (bufByte >> (7 - bitPos)) & 0x1;
      if (is_block_free == 1)
      {
        continue;
      } 
      else
      {
        //switch the target bit to 1
        bufByte = (bufByte) | (0x1 << (7 - bitPos));

        per_block_bitmap[bytePos] = bufByte;
        d->write_block(block_index, per_block_bitmap);

        delete[] per_block_bitmap;
        return (block_index - 2) * (8 * BLOCK_SIZE) + block_bitmap_index;
      }
    }

    //reset the start blockid in a block
    block_bitmap_index = 0;
  }
  delete[] per_block_bitmap;
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  //get the target block in bitmap
  blockid_t target_bitmap_block = BBLOCK(id);
  //malloc a space to store the block in bitmap
  char *per_block_bitmap=(char *)malloc(BLOCK_SIZE);
  read_block(target_bitmap_block, per_block_bitmap);
  //get the index of target bit in a block
  uint32_t bitmap_index = id % (8 * BLOCK_SIZE);
  //get the index of target byte in a block
  uint32_t byte_index = bitmap_index / 8;
  //get the index of target bit in a byte
  uint32_t bit_index = bitmap_index % 8;
  //target byte
  char target_byte = per_block_bitmap[byte_index];
  //swith the target bit to 0
  target_byte = target_byte & (~(0x1 << (7 - bit_index)));
  per_block_bitmap[byte_index] = target_byte;
  write_block(target_bitmap_block, per_block_bitmap);
  
  delete[] per_block_bitmap;
  
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  uint32_t i_inode=1; //inode index counter
  for(;i_inode<(bm->sb).ninodes;i_inode++)
  {
    struct inode *tmp_inode=get_inode(i_inode);
    if(tmp_inode==NULL) //init a new block
    {
      struct inode *new_inode=(inode *)malloc((sizeof(inode)));
      //set default value
      new_inode->type=type;
      new_inode->size=0;
      new_inode->atime=time(NULL);
      new_inode->mtime=time(NULL);
      new_inode->ctime=time(NULL);
      //store newly alloced block
      put_inode(i_inode,new_inode);
      
      return i_inode;
    }
    else
    {
      delete tmp_inode;
      //avoid wild pointer
      tmp_inode=NULL;
    }

  }

  //std::cout<<"inode_manager::alloc_inode: Error,all blocks have been alloced"<<std::endl;
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  struct inode *target_inode=get_inode(inum);  //get target inode
  
  if(target_inode!=NULL)
  {
    //chear the target inode
    target_inode->type=0;
    target_inode->size=0;
    // target_inode->atime=0;
    // target_inode->mtime=0;
    // target_inode->ctime=0;
    memset(target_inode, 0, sizeof(inode_t));
    //write back
    put_inode(inum,target_inode);

    delete target_inode;
    //avoid wild pointer
    target_inode=NULL;
  }
  else
  {
    delete target_inode;
    //avoid wild pointer
    target_inode=NULL;

    //std::cout<<"inode_manager::free_inode: Target Block is NULL"<<std::endl;
  }

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;
  std::cout<<"inode_manager::put_inode inum:"<<IBLOCK(inum, bm->sb.nblocks)<<std::endl;
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  
  struct inode *target_inode=get_inode(inum);
  if(target_inode==NULL)
  {
    delete target_inode;
    printf("inode_manager::read_file:target_inode is NULL\n");
    return;
  }
  else
  {
    //get inode attr size
    *size=target_inode->size;

    if(target_inode->size<=BLOCK_SIZE)
    {
      //malloc new space for buf_out(at least BLOCK_SIZE)
      *buf_out=(char *)malloc(BLOCK_SIZE);
      //read content in block
      bm->read_block(target_inode->blocks[0], *buf_out);
    }
    else
    {
      //calculate read time(block num)
      unsigned int read_time=((target_inode->size)%BLOCK_SIZE==0)?((target_inode->size)/BLOCK_SIZE):((target_inode->size)/BLOCK_SIZE+1);
      //alloc the buf_out according to the size
      *buf_out=(char *)malloc(BLOCK_SIZE*read_time);
      //no indirect block
      if(read_time<=NDIRECT)
      {
        int buf_offset=0;
        for(unsigned int i=0;i<read_time;i++)
        {
          bm->read_block(target_inode->blocks[i],*buf_out+buf_offset);
          buf_offset+=BLOCK_SIZE;
        }
      }
      //has indirect block
      else
      { 
        int buf_offset=0;

        //read direct block
        for(int i=0;i<NDIRECT;i++)
        {
          bm->read_block(target_inode->blocks[i],*buf_out+buf_offset);
          buf_offset+=BLOCK_SIZE;//move forward offset
        }
        //handle indirect block
        //get indirect block id
        blockid_t *indirect_block_id=(blockid_t *)malloc(BLOCK_SIZE);
        bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_block_id);
        //read indirect block
        for(unsigned int i=0;i<read_time-NDIRECT;i++)
        {
          bm->read_block(indirect_block_id[i],*buf_out+buf_offset);
          buf_offset+=BLOCK_SIZE;//move forward offset
        }
        
        delete []indirect_block_id;
      } 
    }  
    //change atime&write back
    target_inode->atime=time(NULL);
    put_inode(inum,target_inode);

    delete target_inode;
    return;
  }
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  std::cout<<"inode_manager::write_file: inum:"<<inum<<std::endl;
  std::cout<<"inode_manager::write_file: content:"<<buf<<std::endl;
  struct inode *target_inode=get_inode(inum);

  char zero[BLOCK_SIZE];
  bzero(zero, sizeof(zero));
  
  if(target_inode==NULL)
  {
    return;
  }
  else
  {
    //std::cout<<"size:"<<size<<std::endl;
    //calculate the block number the file need
    unsigned int need_block_num=((size%BLOCK_SIZE)==0)?(size/BLOCK_SIZE):(size/BLOCK_SIZE+1);
    //std::cout<<"need_block_num:"<<need_block_num<<std::endl;
    //judge whether the file size is too large
    if(need_block_num>MAXFILE)
    {
      printf("inode_manager::write_file: Error,too large file");
      return;
    }
    else
    {
      //calculate origin block number in target inode
      unsigned int origin_block_num=((target_inode->size)%BLOCK_SIZE==0)?((target_inode->size)/BLOCK_SIZE):((target_inode->size)/BLOCK_SIZE+1);
      std::cout<<"inode_manager::write_file: origin_block_num:"<<origin_block_num<<std::endl;
      std::cout<<"inode_manager::write_file: need_block_num:"<<need_block_num<<std::endl;
      //need<NDIRECT:only direct block
      if(need_block_num<=NDIRECT)
      {
        //case 1:origin<need-----alloc new block
        if(origin_block_num<need_block_num)
        {
          std::cout<<"inode_manager::write_file: case1-1"<<std::endl;
          //alloc new block
          for(unsigned int i=origin_block_num;i<need_block_num;i++)
          {
            
            target_inode->blocks[i] = bm->alloc_block();
            std::cout<<"inode_manager::Alloc new block: new_block_id:"<<target_inode->blocks[i]<<std::endl;
          }
          //write block
          int buf_offset=0; 
          for(unsigned int i=0;i<need_block_num;i++)
          {
            std::cout<<"inode_manager::Write block: write_block_id:"<<target_inode->blocks[i]<<std::endl;
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);
          std::cout<<"inode_manager::write_file: case1-1: inum:"<<inum<<std::endl;
          put_inode(inum, target_inode);
          delete target_inode;
          return;
        }
        //case 2:origin=need
        else if(origin_block_num==need_block_num)
        {
          std::cout<<"inode_manager::write_file: case1-2"<<std::endl;
          //write block
          int buf_offset=0;
          for(unsigned int i=0;i<need_block_num;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);
          std::cout<<"inode_manager::write_file: case1-2: inum:"<<inum<<std::endl;
          put_inode(inum, target_inode);
          delete target_inode;
          return;
        }
        //case 3:NDIRECT>=origin>need-----free reduntant blocks
        else if(origin_block_num>need_block_num&&origin_block_num<=NDIRECT)
        {
          std::cout<<"inode_manager::write_file: case1-3"<<std::endl;
          //free reduntant block
          for(unsigned int i=need_block_num;i<origin_block_num;i++)
          {
            bm->free_block(target_inode->blocks[i]);
          }
          //write block
          int buf_offset=0;
          for(unsigned int i=0;i<need_block_num;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);
          std::cout<<"inode_manager::write_file: case1-3: inum:"<<inum<<std::endl;
          put_inode(inum, target_inode);
          delete target_inode;
          return;
        }
        //case 4:origin>NDIRECT>need-----free reduntant blocks
        else 
        {
          std::cout<<"inode_manager::write_file: case1-4"<<std::endl;
          //get block contain indirect blockid list
          blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
          bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);

          //free reduntant indirect blocks
          for (unsigned int i = 0; i < origin_block_num - NDIRECT; i++)
          {
            bm->free_block(indirect_blockid[i]);
          }
          //free reduntant direct blocks
          for (int i=need_block_num;i<=NDIRECT;i++)
          {
            bm->free_block(target_inode->blocks[i]);
          }
          //write block
          int buf_offset=0;
          for(unsigned int i=0;i<need_block_num;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);

          put_inode(inum, target_inode);
          delete[] indirect_blockid;
          delete target_inode;
          return;
        }
      }
      //need>NDIRECT:need indirect block
      else
      {
        //case 1:need>NDIRECT>origin
        if(need_block_num>NDIRECT&&origin_block_num<NDIRECT)
        {
          std::cout<<"inode_manager::write_file: case2-1"<<std::endl;
          //alloc new direct block
          for(int i=origin_block_num;i<NDIRECT;i++)
          {
            target_inode->blocks[i] = bm->alloc_block();
          }
          //malloc block contain indirect blockid
          blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
          //alloc the direct block containing indirect block
          target_inode->blocks[NDIRECT] = bm->alloc_block();
          //alloc new indirect block
          for(unsigned int i=0;i<need_block_num-NDIRECT;i++)
          {
            indirect_blockid[i] = bm->alloc_block();
          }

          //write direct block
          int buf_offset=0;
          for(int i=0;i<NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect block
          for(unsigned int i=0;i<need_block_num-NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(indirect_blockid[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect blockid in direct block
          bm->write_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);

          //set other attr
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);

          put_inode(inum, target_inode);
          delete[] indirect_blockid;
          delete target_inode;
          return;
        }
        //case 2::need>origin>=NDIRECT
        else if(need_block_num>origin_block_num&&origin_block_num>=NDIRECT)
        {
          std::cout<<"inode_manager::write_file: case2-2"<<std::endl;
          //malloc block contain indirect blockid
          blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
          //read old indirect block
          bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);
          //alloc new indirect block
          for(unsigned int i=origin_block_num;i<need_block_num;i++)
          {
            indirect_blockid[i-NDIRECT] = bm->alloc_block();
          }

          //write direct block
          int buf_offset=0;
          for(int i=0;i<NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect block
          for(unsigned int i=0;i<need_block_num-NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(indirect_blockid[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect blockid in direct block
          bm->write_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);

          //set other attr
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);

          put_inode(inum, target_inode);
          delete[] indirect_blockid;
          delete target_inode;
          return;
        }
        //case 3:need=origin>NDIRECT
        else if(need_block_num==origin_block_num)
        {
          std::cout<<"inode_manager::write_file: case2-3"<<std::endl;
          //malloc block contain indirect blockid
          blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
          //read old indirect block
          bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);
          //write direct block
          int buf_offset=0;
          for(int i=0;i<NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect block
          for(unsigned int i=0;i<need_block_num-NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(indirect_blockid[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect blockid in direct block
          bm->write_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);

          //set other attr
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);

          put_inode(inum, target_inode);
          delete[] indirect_blockid;
          delete target_inode;
          return;
        }
        //case 4:origin>need>NDIRECT
        else
        {
          std::cout<<"inode_manager::write_file: case2-4"<<std::endl;
          //malloc block contain indirect blockid
          blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
          //read old indirect block
          bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);
          //free reduntant indirect blocks
          for (unsigned int i = need_block_num-NDIRECT; i < origin_block_num-NDIRECT; i++)
          {
            bm->free_block(indirect_blockid[i]);
          }
          //write direct block
          int buf_offset=0;
          for(int i=0;i<NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(target_inode->blocks[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect block 
          for(unsigned int i=0;i<need_block_num-NDIRECT;i++)
          {
            bm->write_block(target_inode->blocks[i],zero);
            bm->write_block(indirect_blockid[i],buf+buf_offset);
            buf_offset+=BLOCK_SIZE;
          }
          //write indirect blockid in direct block
          bm->write_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);

          //set other attr
          target_inode->size=size;

          //change time
          target_inode->ctime=time(NULL);
          target_inode->mtime=time(NULL);

          put_inode(inum, target_inode);
          delete[] indirect_blockid;
          delete target_inode;
          return;
        }
      }
    } 
  }
  //target_inode->ctime=target_inode->mtime=time(NULL);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  
  struct inode *target_inode=get_inode(inum);
  if(target_inode==NULL)
  {
    //std::cout<<"inode_manager::getattr: Target inode is NULL"<<std::endl;
    a.type=0;
    a.atime=0;
    a.mtime=0;
    a.ctime=0;
    a.size=0;
  }
  else
  {
    //copy attributes
    a.type=target_inode->type;
    a.atime=target_inode->atime;
    a.mtime=target_inode->atime;
    a.ctime=target_inode->ctime;
    a.size=target_inode->size;
  }
  
  delete target_inode;
  target_inode=NULL;

  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  
  struct inode *target_inode=get_inode(inum);
  if(target_inode==NULL)
  {
    printf("inode_manager::remove_file:The inode is null,the file does not exsist");
    delete target_inode;
    return;
  }
  else
  {
    //calculate block num
    unsigned int target_block_num=((target_inode->size)%BLOCK_SIZE?((target_inode->size)/BLOCK_SIZE):((target_inode->size)/BLOCK_SIZE+1));
    //no indirect block
    if(target_block_num<NDIRECT)
    {
      //free direct block
      for(unsigned int i=0;i<target_block_num;i++)
      {
        bm->free_block(target_inode->blocks[i]);
      }
      //free inode
      free_inode(inum);
    }
    //has indirect block
    else
    {
      //get indirect block id list
      blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
      bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);
      //free indirect block
      for(unsigned int i=0;i<target_block_num-NDIRECT;i++)
      {
        bm->free_block(indirect_blockid[i]);
      }
      //free direct block
      for(int i=0;i<=NDIRECT;i++)
      {
        bm->free_block(target_inode->blocks[i]);
      }
      //free inode
      free_inode(inum);

      delete[] indirect_blockid;
    }
  }
  
  return;
}

void
inode_manager::append_block(uint32_t inum, blockid_t &bid)
{
  /*
   * your code goes here.
   */

  //judge whether the inode already exsist
  struct inode *target_inode=get_inode(inum);
  //if not error
  if(target_inode==NULL) //init a new block
  {
    std::cout<<"inode_manager::append_block: Target inode block already exsist"<<std::endl;
    return;
  }
  else
  {
    //calculate the block index
    int block_index=0;
    if(target_inode->size%BLOCK_SIZE==0)
    {
      block_index=target_inode->size/BLOCK_SIZE;
    }
    else
    {
      block_index=target_inode->size/BLOCK_SIZE+1;
    }
    //judge the size of the file
    if(block_index>MAXFILE)
    {
      std::cout<<"inode_manager::append_block: Target inode alloc too many blocks"<<std::endl;
      return;
    }

    bid=bm->alloc_block();
    //append the block
    if(block_index<NDIRECT)
    {
      target_inode->blocks[block_index]=bid;
    }
    else
    {
      //malloc block contain indirect blockid
      blockid_t *indirect_blockid = (blockid_t *)malloc(BLOCK_SIZE);
      if(block_index==NDIRECT)
      {
        target_inode->blocks[NDIRECT]=bm->alloc_block();
      }
      //read old indirect block
      bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);
      //append new block
      indirect_blockid[block_index-NDIRECT]=bid;
      bm->write_block(target_inode->blocks[NDIRECT],(char *)indirect_blockid);
    }
    target_inode->size+=BLOCK_SIZE;
    put_inode(inum,target_inode);
  }
  delete target_inode;
  //avoid wild pointer
  target_inode=NULL;
}

void
inode_manager::get_block_ids(uint32_t inum, std::list<blockid_t> &block_ids)
{
  /*
   * your code goes here.
   */
  //get target inode
  struct inode *target_inode=get_inode(inum);
  //calculate the block_num
  unsigned int block_num=((target_inode->size)%BLOCK_SIZE==0)?((target_inode->size)/BLOCK_SIZE):((target_inode->size)/BLOCK_SIZE+1);
  //no inndirect block
  if(block_num<=NDIRECT)
  {
    for(int i=0;i<block_num;i++)
    {
      block_ids.push_back(target_inode->blocks[i]);
    }
  }
  //has indirect block
  else
  {
    //add direct block_num to the list
    for(int i=0;i<NDIRECT;i++)
    {
      block_ids.push_back(target_inode->blocks[i]);
    }
    //get indirect block id
    blockid_t *indirect_block_id=(blockid_t *)malloc(BLOCK_SIZE);
    bm->read_block(target_inode->blocks[NDIRECT],(char *)indirect_block_id);
    //read indirect block
    for(unsigned int i=0;i<block_num-NDIRECT;i++)
    {
      block_ids.push_back(indirect_block_id[i]);
    }
    
    delete []indirect_block_id;
  }

}

void
inode_manager::read_block(blockid_t id, char buf[BLOCK_SIZE])
{
  /*
   * your code goes here.
   */
  bm->read_block(id, buf);
}

void
inode_manager::write_block(blockid_t id, const char buf[BLOCK_SIZE])
{
  /*
   * your code goes here.
   */
  bm->write_block(id,buf);

}

void
inode_manager::complete(uint32_t inum, uint32_t size)
{
  /*
   * your code goes here.
   */
  //get target inode
  struct inode *target_inode=get_inode(inum);
  //updata meta data
  target_inode->size=size;
  target_inode->atime=time(NULL);
  target_inode->mtime=time(NULL);
  target_inode->ctime=time(NULL);
  //put back the updated inode
  put_inode(inum,target_inode);
}
