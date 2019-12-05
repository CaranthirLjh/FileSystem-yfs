#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct symlinkinfo
  {
    unsigned long long size;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

 public:
  yfs_client(std::string, std::string);
  yfs_client(extent_client*, lock_client*);

  bool nlock_isfile(inum);
  bool isfile(inum);
  bool nlock_isdir(inum);
  bool isdir(inum);
  bool nlock_islink(inum inum);
  bool islink(inum inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  //lookup is used by other function, use nlock_lookup to avoid repetitive lock
  int nlock_lookup(inum, const char *, bool &, inum &);
  int lookup(inum, const char *, bool &, inum &);
  int nlock_create(inum, const char *, mode_t, inum &);
  int create(inum, const char *, mode_t, inum &);
  int nlock_readdir(inum, std::list<dirent> &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int nlock_unlink(inum,const char *);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  int createlink(inum parent ,std::string link_name, std::string target_name, inum &ino_out);
  int readlink(inum inum ,std::string &link_name);
  
  /** you may need to add symbolic link related methods here.*/
};

#endif 