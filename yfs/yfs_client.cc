// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//name space
using std::cout;
using std::endl;
using std::string;
using std::istringstream;

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    // lc = new lock_client(lock_dst);
    lc = new lock_client_cache(lock_dst);
    // lc->acquire(1);
    // if (ec->put(1, "") != extent_protocol::OK)
    //     printf("error init root dir\n"); // XYB: init root dir
    // lc->release(1);
}
yfs_client::yfs_client(extent_client* new_ec, lock_client* new_lc)
{
    ec=new_ec;
    lc=new_lc;
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::nlock_isfile(inum inum)
{
    cout<<"yfs_client::nlock_isfile: Start! inum:"<<inum<<endl;

    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        cout<<"yfs_client::nlock_isfile: Is a file! inum:"<<inum<<endl;
        return true;
    } 
    cout<<"yfs_client::nlock_isfile: Is not a file! inum:"<<inum<<endl;

    return false;
}

bool
yfs_client::isfile(inum inum)
{
    cout<<"yfs_client::isfile: Start! inum:"<<inum<<endl;
    lc->acquire(inum);
    int r=nlock_isfile(inum);
    lc->release(inum);
    return r;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */
bool
yfs_client::nlock_isdir(inum inum)
{
    cout<<"yfs_client::nlock_isdir: Start! inum:"<<inum<<endl;
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        cout<<"yfs_client::nlock_isdir: Is a dir! inum:"<<inum<<endl;
        return true;
    } 
    cout<<"yfs_client::nlock_isdir: Is not a dir! inum:"<<inum<<endl;

    return false;
}

bool
yfs_client::isdir(inum inum)
{
    cout<<"yfs_client::isdir: Start! inum:"<<inum<<endl;
    lc->acquire(inum);
    int r=nlock_isdir(inum);
    lc->release(inum);
    return r;
}

bool
yfs_client::nlock_islink(inum inum)
{
    cout<<"yfs_client::nlock_islink: Start! inum:"<<inum<<endl;

    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        cout<<"yfs_client::nlock_islink: Is a link! inum:"<<inum<<endl;
        return true;
    } 
    cout<<"yfs_client::nlock_islink: Is not a link! inum:"<<inum<<endl;

    return false;
}

bool
yfs_client::islink(inum inum)
{
    cout<<"yfs_client::nlock_islink: Start! inum:"<<inum<<endl;
    lc->acquire(inum);
    int r=nlock_islink(inum);
    lc->release(inum);
    return r;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    cout<<"yfs_client::getfile: Start! inum:"<<inum<<endl;
    lc->acquire(inum);

    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:

    lc->release(inum);

    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    cout<<"yfs_client::getdir: Start! inum:"<<inum<<endl;
    lc->acquire(inum);

    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:

    lc->release(inum);

    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    cout<<"yfs_client::setattr: Start! inum:"<<ino<<endl;
    lc->acquire(ino);

    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    //create a string to store the inode content
    string inode_content="";
    if(ec->get(ino,inode_content)==extent_protocol::OK)
    {
        inode_content.resize(size);
        ec->put(ino,inode_content);
    }
    else
    {
        cout<<"yfs_client::setattr: Read content error."<<endl;
        r=IOERR;
    }

    lc->release(ino);

    return r;
}

int yfs_client::nlock_create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    cout<<"yfs_client::nlock_create: Start! parent:"<<parent<<" name:"<<name<<endl;
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    
    //check the file exsist or not
    bool found=false;
    nlock_lookup(parent,name,found,ino_out);
    //the file already exsist
    if(found==true)
    {
        cout<<"yfs_client::nlock_create: File already exsist! parent:"<<parent<<" name:"<<name<<endl;
        r=EXIST;
    }
    //create a new dir item
    else
    {
        //create a new inode for new file
        ec->create(extent_protocol::T_FILE,ino_out);

        
        //get present dir items
        string dir_list="";
        if(ec->get(parent,dir_list)==extent_protocol::OK)
        {
            cout<<"yfs_client::nlock_create: Old dir_list:"<<dir_list<<endl;
            string file_name=name;
            //insert the new dir item
            if(dir_list.size()==0)
            {
                //dir_list.append(string(name) + ',' + filename(ino_out));
                dir_list+=(file_name+";"+filename(ino_out));
            }
            else
            {
                //dir_list.append(',' + string(name) + ',' + filename(ino_out));
                dir_list+=(";"+file_name+";"+filename(ino_out));
            }
            ec->put(parent,dir_list);
            cout<<"yfs_client::nlock_create: New dir_list:"<<dir_list<<endl;
        }
        cout<<"yfs_client::nlock_create: File successfully created! parent:"<<parent<<" name:"<<name<<endl;
    }

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    cout<<"yfs_client::create: Start! inum:"<<parent<<endl;
    lc->acquire(parent);

    int r = nlock_create(parent,name,mode,ino_out);
    cout<<"yfs_client::create: File successfully created! parent:"<<parent<<" name:"<<name<<endl;
    lc->release(parent);

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    cout<<"yfs_client::mkdir: Start! inum:"<<parent<<endl;
    lc->acquire(parent);

    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool found=false;
    nlock_lookup(parent,name,found,ino_out);
    //the file already exsist
    if(found==true)
    {
        cout<<"yfs_client::mkdir: Dir already exsist! parent:"<<parent<<" name:"<<name<<endl;
        r=EXIST;
    }
    //create a new dir item
    else
    {
        //create a new inode for new file
        ec->create(extent_protocol::T_DIR,ino_out);
        cout<<"yfs_client::mkdir: Dir name! dir_name:"<<name<<endl;
        cout<<"yfs_client::mkdir: Dir ino! ino_out:"<<ino_out<<endl;
        //get present dir items
        string dir_list="";
        if(ec->get(parent,dir_list)==extent_protocol::OK)
        {
            string file_name=name;
            //insert the new dir item
            if(dir_list.size()==0)
            {
                dir_list+=(file_name+";"+filename(ino_out));
            }
            else
            {
                dir_list+=(";"+file_name+";"+filename(ino_out));
            }
            cout<<"yfs_client::mkdir: Write dir entry! dir_list:"<<dir_list<<endl;
            ec->put(parent,dir_list);
        }
        cout<<"yfs_client::mkdir: Dir successfully created! parent:"<<parent<<" name:"<<name<<endl;
    }

    lc->release(parent);

    return r;
}

int
yfs_client::nlock_lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    cout<<"yfs_client::nlock_lookup: Start! inum:"<<parent<<endl;

    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    //judege whether the parent is a dir
    if(!nlock_isdir(parent))
    {
        cout<<"yfs_client::nlock_lookup: The parent is not a dir."<<endl;
        found=false;
        return r;
    }
    else
    {
        //store the dir list read from the parent
        string dir_list;
        if(ec->get(parent,dir_list)==extent_protocol::OK)
        {
            //create string to store file name&inode num
            string file_name="";
            string inode_num="";
            //set three read states and init
            enum readstate{
                FILENAME,
                INODENUM,
                CHECK
            };
            readstate read_state=FILENAME;

            //the length of dir list
            unsigned int dir_list_len=dir_list.length();
            for(unsigned int pos=0;pos<dir_list_len;pos++)
            {
                if(read_state==FILENAME)
                {
                    //meet the , next is inode num
                    if(dir_list[pos]==';')
                    {
                        read_state=INODENUM;
                        pos+=1;
                    }
                    else
                    {
                        file_name+=dir_list[pos];
                    }
                }

                if(read_state==INODENUM)
                {
                    //meet the , next is check
                    if(dir_list[pos]==';')
                    {
                        read_state=CHECK;
                    }
                    //meet the end , next is check
                    else if(pos==dir_list_len-1)
                    {
                        inode_num+=dir_list[pos];
                        read_state=CHECK;
                    }
                    else
                    {
                        inode_num+=dir_list[pos];
                    }
                }

                if(read_state==CHECK)
                {
                    string search_name=name;
                    if(file_name==search_name)
                    {
                        cout<<"yfs_client::nlock_lookup: Found target file."<<endl;
                        //set found
                        found=true;
                        //set ino_out
                        ino_out=n2i(inode_num);

                        return r;
                    }
                    //else reser the state and store space
                    read_state=FILENAME;
                    file_name="";
                    inode_num="";
                }
            }
            r=NOENT;
            cout<<"yfs_client::nlock_lookup: Can not find target file."<<endl;
            found=false;
        }
        //read dir list failed
        else
        {
            cout<<"yfs_client::nlock_lookup: Read dir error."<<endl;
            r=IOERR;
        }
    }

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    cout<<"yfs_client::lookup: Start! parent:"<<parent<<" name:"<<name<<endl;
    // lc->acquire(parent);
    int r=nlock_lookup(parent,name,found,ino_out);
    // lc->release(parent);
    return r;
}

int yfs_client::nlock_readdir(inum dir, std::list<dirent> &list)
{
    cout<<"yfs_client::nlock_readdir: Start! dir:"<<dir<<endl;
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    //judege whether the parent is a dir
    if(!nlock_isdir(dir))
    {
        cout<<"yfs_client::readdir: The parent is not a dir."<<endl;
        lc->release(dir);
        return r;
    }
    else
    {
        //get dir_list
        string dir_list="";
        if(ec->get(dir,dir_list)==extent_protocol::OK)
        {
            cout<<"yfs_client::readdir: dir_list:"<<dir_list<<endl;
            //create string to store file name&inode num
            string file_name="";
            string inode_num="";
            //set three read states and init
            enum readstate{ 
                FILENAME,
                INODENUM,
                STORE
            };
            readstate read_state=FILENAME;
            //the length of dir list
            int dir_list_len=dir_list.length();
            for(int pos=0;pos<dir_list_len;pos++)
            {
                if(read_state==FILENAME)
                {
                    //meet the , next is inode num
                    if(dir_list[pos]==';')
                    {
                        read_state=INODENUM;
                        pos+=1;
                    }
                    else
                    {
                        file_name+=dir_list[pos];
                    }
                }

                if(read_state==INODENUM)
                {
                    //meet the , next is check
                    if(dir_list[pos]==';')
                    {
                        read_state=STORE;
                    }
                    //meet the end , next is check
                    else if(pos==dir_list_len-1)
                    {
                        inode_num+=dir_list[pos];
                        read_state=STORE; 
                    }
                    else
                    {
                        inode_num+=dir_list[pos];
                    }
                    
                }

                if(read_state==STORE)
                {
                    cout<<"yfs_client::readdir: file_name:"<<file_name<<endl;
                    cout<<"yfs_client::readdir: inode_num:"<<inode_num<<endl;
                    dirent *tmp_dir=new dirent();

                    //store dir
                    inum tmp_inode;
                    istringstream iss(inode_num);
                    iss>>tmp_inode;

                    tmp_dir->name=file_name;
                    tmp_dir->inum=tmp_inode;
                    list.push_back(*tmp_dir);
                    cout<<"yfs_client::readdir: name:"<<file_name<<",inum:"<<tmp_inode<<endl;
                    
                    //reset val
                    file_name="";
                    inode_num="";
                    read_state=FILENAME;
                    delete tmp_dir;
                }
            }
            cout<<"yfs_client::readdir: Read dir success."<<endl;
        }
        else
        {
            cout<<"yfs_client::readdir: Read dir error."<<endl;
            r=IOERR;
        }
    }

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    cout<<"yfs_client::readdir: Start! inum:"<<dir<<endl;
    lc->acquire(dir);

    int r = nlock_readdir(dir,list);

    lc->release(dir);

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    cout<<"yfs_client::read: Start! inum:"<<ino<<endl;
    lc->acquire(ino);

    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */

    string read_content="";
    if(ec->get(ino,read_content)==extent_protocol::OK)
    {
        unsigned int content_size=read_content.size();
        //case 1:off>content size
        if((unsigned int)off>=content_size)
        {
            read_content="";
        }
        else 
        {
            read_content=read_content.substr(off);
        }

        if(read_content.size()>size)
        {
            read_content.resize(size);
        }
        data=read_content;
        cout<<"yfs_client::read: read_content:"<<data<<endl;
        cout<<"yfs_client::read: Read content success."<<endl;
    }
    else
    {
        cout<<"yfs_client::read: Read content error."<<endl;
        r=IOERR;
    }

    lc->release(ino);

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    cout<<"yfs_client::write: Start! inum:"<<ino<<endl;
    cout<<"yfs_client::write: size:"<<size<<endl;
    cout<<"yfs_client::write: off:"<<off<<endl;
    cout<<"yfs_client::write: content:"<<data<<endl;
    lc->acquire(ino);

    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    bytes_written = 0;
    string read_content="";
    if(ec->get(ino,read_content)==extent_protocol::OK)
    {
        cout<<"yfs_client::write: Get content:"<<read_content<<endl;
        string new_content(size,0);
        for(int i=0;i<size;i++)
        {
            new_content[i]=data[i];
        }
        new_content=new_content.substr(0,size);

        if(read_content.size()<off)
        {
            bytes_written+=(off-read_content.size());
            read_content.append(off-read_content.size(),0);
            read_content+=new_content;
        }
        else
        {
            read_content.replace(off,size,new_content);
        }

        bytes_written+=size;
        cout<<"yfs_client::write: Before put: ino:"<<ino<<" content:"<<read_content<<endl;
        ec->put(ino,read_content);
        cout<<"yfs_client::write: Write content:"<<new_content<<endl;

        cout<<"yfs_client::write: Write content success."<<endl;
    }
    else
    {
        cout<<"yfs_client::write: Read content error."<<endl;
        r=IOERR;
    }

    lc->release(ino);

    return r;
}

int yfs_client::nlock_unlink(inum parent,const char *name)
{
    cout<<"yfs_client::nlock_unlink: Start! parent:"<<parent<<"name:"<<name<<endl;
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    bool found=false;
    inum target_inode=0;
    nlock_lookup(parent,name,found,target_inode);

    if(found==true)
    {
        //create a space to store dir item
        string dir_list="";
        if(ec->get(parent,dir_list)==extent_protocol::OK)
        {
            
            //clear the item in dir:
            //locate the begin of target item
            size_t begin_pos=dir_list.find(name);
            //calculate the target length
            string target_name=name;
            size_t target_len=target_name.size()+1+(filename(target_inode)).size();
            //clear the target item
            //case 1:the item is the first&last one;
            if(begin_pos==0&&(dir_list.length()==begin_pos+target_len))
            {
                dir_list.erase(begin_pos,target_len);
            }
            //case 2:the item is the last one;
            else if(dir_list.length()==begin_pos+target_len)
            {
                dir_list.erase(begin_pos-1,target_len+1);
            }
            //case 3:else
            else
            {
                dir_list.erase(begin_pos,target_len+1);
            }

            if(dir_list[0]=='\0')
            {
                dir_list="";
            }

            ec->put(parent,dir_list);
            cout<<"yfs_client::nlock_unlink: New dir_list:"<<dir_list<<endl;
            cout<<"yfs_client::nlock_unlink: Dir_list size:"<<dir_list.size()<<endl;
            cout<<"yfs_client::nlock_unlink: Read content success."<<endl;
            //remove the target inode 
            ec->remove(target_inode);
        }
        else
        {
            cout<<"yfs_client::nlock_unlink: Read content error."<<endl;
            r=IOERR;
        }
    }
    else
    {
        cout<<"yfs_client::nlock_unlink: Can't find link."<<endl;
        r=NOENT;
    }

    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    cout<<"yfs_client::unlink: Start! inum:"<<parent<<endl;
    lc->acquire(parent);

    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    bool found=false;
    inum target_inode=0;
    nlock_lookup(parent,name,found,target_inode);

    if(found==true)
    {
        //create a space to store dir item
        string dir_list="";
        if(ec->get(parent,dir_list)==extent_protocol::OK)
        {
            
            //clear the item in dir:
            //locate the begin of target item
            size_t begin_pos=dir_list.find(name);
            //calculate the target length
            string target_name=name;
            size_t target_len=target_name.size()+1+(filename(target_inode)).size();
            //clear the target item
            //case 1:the item is the first&last one;
            if(begin_pos==0&&(dir_list.length()==begin_pos+target_len))
            {
                dir_list.erase(begin_pos,target_len);
            }
            //case 2:the item is the last one;
            else if(dir_list.length()==begin_pos+target_len)
            {
                dir_list.erase(begin_pos-1,target_len+1);
            }
            //case 3:else
            else
            {
                dir_list.erase(begin_pos,target_len+1);
            }
            cout<<"yfs_client::unlink: dir_list:"<<dir_list<<endl;
            if(dir_list[0]=='\0')
            {
                dir_list="";
            }
            ec->put(parent,dir_list);
            //remove the target inode 
            lc->acquire(target_inode);
            ec->remove(target_inode);
            lc->release(target_inode);
            cout<<"yfs_client::unlink: Read content success."<<endl;
        }
        else
        {
            cout<<"yfs_client::unlink: Read content error."<<endl;
            r=IOERR;
        }
    }
    else
    {
        cout<<"yfs_client::unlink: Can't find link."<<endl;
        r=NOENT;
    }

    lc->release(parent);

    return r;
}

//symbolic link functions:

int
yfs_client::createlink(inum parent , string link_name, string target_name, inum &ino_out)
{
    cout<<"yfs_client::createlink: Start! parent:"<<parent<<endl;
    lc->acquire(parent);

    int r = OK;

    //check whether the file is exsist
    bool found=false;
    const char* tmp_link_name=link_name.c_str();
    nlock_lookup(parent,tmp_link_name,found,ino_out); 
    if(found==true)
    {
        cout<<"yfs_client::createlink: Link already exsist! parent:"<<parent<<endl;
        r=EXIST;
    }
    else
    {
        //create a new inode for new link
        ec->create(extent_protocol::T_SYMLINK,ino_out);

        //get present dir items&add new item
        string dir_list="";
        if(ec->get(parent,dir_list)==extent_protocol::OK)
        {
            string file_name=link_name;
            //insert the new dir item
            if(dir_list.size()==0)
            {
                dir_list=file_name+";"+filename(ino_out);
            }
            else
            {
                dir_list=dir_list+";"+file_name+";"+filename(ino_out);
            }
            ec->put(parent,dir_list);
        }

        //add new link
        ec->put(ino_out,target_name);
        cout<<"yfs_client::createlink: Link successfully created! parent:"<<parent<<endl;
    }

    lc->release(parent);

    return r;
}

int
yfs_client::readlink(inum inum ,string &link_name)
{
    cout<<"yfs_client::readlink: Start! inum:"<<inum<<endl;
    lc->acquire(inum);

    int r = OK;
    if (ec->get(inum, link_name) != extent_protocol::OK) {
        printf("error getting attr\n");
        r=IOERR;
    }

    lc->release(inum);
    cout<<"yfs_client::readlink: Success! inum:"<<inum<<endl;
    return r;
}
