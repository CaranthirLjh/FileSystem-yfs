# yfs lab
 MIT 6.033, implement a distributed file system
  - To run this lab， you need to use a computer that has the FUSE module, library, and headers installed
  - You should be able to install these on your own machine by following the instructions at [fuse.sourceforge.net](https://github.com/libfuse/libfuse).

 # lab 1 & lab 2
 ## Implement a basic file system
  - Mainly focus on implementing parts shown in this image
  ![Lab1 Part 2](https://github.com/TactfulYuu/yfs-lab/blob/master/img/lab1_part2_structure.png)
 ### Implement an inode manager to support the file system
  - In this part, I implement functions in Inode_manager.cc & Extent_server.cc & Extent_client.cc to provide support for yfs APIs
  - Implement disk::read_block and disk::write_block inode_manager::alloc_inode andinode_manager::getattr, to support CREATE and GETATTR APIs
  - implement inode_manager::write_file, inode_manager::read_file, block_manager::alloc_block, block_manager::free_block, to support PUT and GET APIs
  -  implement inode_manager::remove_file and inode_manager::free_inode, to support REMOVE API
 ### Start the file system implementation by getting some basic operations to work
  - In this part, I implement yfs APIs in Yfs_client.cc based on the functions in Inode_manager.cc & Extent_server.cc & Extent_client.cc
  - yfs APIs: CREATE/MKNOD, LOOKUP, READDIR, SETATTR, WRITE, READ, MKDIR, UNLINK, SYMLINK and READLINK
  
