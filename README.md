# yfs lab
 MIT 6.033, implement a distributed file system
  - To run this lab， you need to use a computer that has the FUSE module, library, and headers installed
  - You should be able to install these on your own machine by following the instructions at [fuse.sourceforge.net](https://github.com/libfuse/libfuse).

    ## lab 1 & lab 2
    ### Implement a basic file system
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
    ### Distributed FileSystem
    - In this part, I write an RPC system to implement distributed file system. A server uses the RPC library by creating an RPC server object listening on a port and registering various RPC handlers. A client creates a RPC client object, asks for it to be connected to the demo_server's address and port, and invokes RPC calls.
    - And I implement a lock server to ensure the correctness of the distributed file system. At any point in time, there is at most one client holding a lock with a given identifier. 
    - The lock server is based on the principle of ticket lock. Each RPC procedure is identified by a unique procedure number. 
    - Acquire lock in the correct place in yfs_client to ensure the atomicity of file system.
    ## lab 3
    ### Design the Protocol
    - First, make a design of the lock state before implement the lock protocol. Here is the five states of lock:
      - none: client knows nothing about this lock
      - free: client owns the lock and no thread has it
      - locked: client owns the lock and a thread has it
      - acquiring: the client is acquiring ownership
      - releasing: the client is releasing ownership
    - Here is some rules of the lock protocol:
      - A single client may have multiple threads waiting for the same lock, but only one thread per client is allowed to be interacting with the server. So only one thread per client can acquire the lock. Once that thread release the lock, it can wake up other threads and one of these threads can acquire the lock. Thread can be identified by its thread id.
      - **Lock Protocol**: When a client asks for a lock with an acquire request, the server grants the lock and responds with OK if the lock is not owned by another client (lock's state is **free**). If the lock's state is not **free**, and there are other clients waiting for the lock, the server responds with a RETRY. Otherwise, the server sends a revoke request to the owner of the lock, and waits for the lock to be released by the owner. Finally, the server sends a retry to the next waiting client, grants the lock and responds with OK.
      - **Lock Cache**: Once a client has acquired a lock, the client the lock in its cache (client keeps the lock instead of sending a release request to the server when a thread of the client releases the lock). The client can grant the lock to other threads on the same client without interacting with the server.
      - **Rovoke Request**: The server sends the client a revoke request to get the lock back. This request tells the client that it should send the lock back to the server when it releases the lock or right now if no thread on the client is holding the lock.
    ## lab 4
    - Deploy this distributed file system to Tencent Cloud Server.
    - Fix some bugs in inode_manager:
      - Change the data structure of bitmap.
      - Update the point when appending blocks. The original design is updating the point after appending is finished. But this design will cause error after consecutive appending.