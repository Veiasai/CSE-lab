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

#define DB 1

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    //lc = new lock_client(lock_dst);
    lc = new lock_client_cache(lock_dst);
}

yfs_client::yfs_client(extent_client * nec, lock_client* nlc){
    ec = nec;
    //lc = new lock_client(lock_dst);
    lc = nlc;
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
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        lc->release(inum);
        return true;
    }else if (a.type == extent_protocol::T_SYMLK) {
        printf("isfile: %lld is a symlink\n", inum);
        lc->release(inum);
        return false;
    } 
    printf("isfile: %lld is a dir\n", inum);
    lc->release(inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */
bool 
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_SYMLK) {
        printf("isfile: %lld is a symlink\n", inum);
        lc->release(inum);
        return true;
    } 
    printf("isfile: %lld is not a symlink\n", inum);
    lc->release(inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        lc->release(inum);
        return true;
    } 
    printf("isfile: %lld is not a dir\n", inum);
    lc->release(inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;
    lc->acquire(inum);
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
yfs_client::getsymlink(inum inum, symlinkinfo &sin1)
{
    int r = OK;
    lc->acquire(inum);
    printf("getlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    sin1.atime = a.atime;
    sin1.mtime = a.mtime;
    sin1.ctime = a.ctime;
    sin1.size = a.size;
    printf("getlink %016llx -> sz %llu\n", inum, sin1.size);
release:
    lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;
    lc->acquire(inum);
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
    #if DB
    std::cout << "setattr:" << ino << " size:" << size << std::endl;
    std::cout.flush();
    #endif

    lc->acquire(ino);
    int r = OK;
    std::string buf;
    r = ec->get(ino, buf);
    buf.resize(size);
    ec->put(ino, buf);
    lc->release(ino);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    #if DB
    std::cout << "create:" << name << " parent:" << parent << std::endl;
    std::cout.flush();
    #endif
  
    int r = OK;
    bool found = false;
    lc->acquire(parent);
    lookup(parent, name, found, ino_out);
    if (found == true){
        lc->release(parent);
        return EXIST;
    }
    ec->create(extent_protocol::T_FILE, ino_out);
    std::string buf;
    r = ec->get(parent, buf);
    buf.insert(buf.size(), name);
    buf.resize(buf.size() + 5, 0);
    *(uint32_t *)(buf.c_str()+buf.size()-4) = ino_out;

    ec->put(parent, buf);
    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    #if DB
    std::cout << "mkdir:" << name << " parent:" << parent << std::endl;
    std::cout.flush();
    #endif

    lc->acquire(parent);
    bool found = false;
    lookup(parent, name, found, ino_out);
    if (found == true){
        lc->release(parent);
        return EXIST;
    }

    int r = OK;
    r = ec->create(extent_protocol::T_DIR, ino_out);
    std::string buf;
    r = ec->get(parent, buf);
    buf.insert(buf.size(), name);
    buf.resize(buf.size() + 5, 0);
    *(uint32_t *)(buf.c_str()+buf.size()-4) = ino_out;

    ec->put(parent, buf);
    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    std::string buf;
    r = ec->get(parent, buf);

    #if DB
    std::cout << "lookup:" << name << " buf:" << buf << std::endl;
    std::cout.flush();
    #endif
    
    int pos = 0;
    while(pos < buf.size()){
        const char * t = buf.c_str()+pos;
        if (strcmp(t, name) == 0){
            ino_out = *(uint32_t *)(t + strlen(t) + 1);
            found = true;
            return r;
        }
        pos += strlen(t) + 1 + sizeof(uint);
    }
    found = false;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    lc->acquire(dir);
    int r = OK;
    std::string buf;
    r = ec->get(dir, buf);
    int pos = 0;
    while(pos < buf.size()){
        struct dirent temp;
        temp.name = std::string(buf.c_str()+pos);
        temp.inum = *(uint32_t *)(buf.c_str() + pos + temp.name.size() + 1);
        list.push_back(temp);
        pos += temp.name.size() + 1 + sizeof(uint32_t);
    }
    lc->release(dir);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    lc->acquire(ino);
    int r = OK;
    r = ec->get(ino, data);

    if (off < data.size()){
        data=data.substr(off);
    }else{
        data="";
    }
    if (data.size() > size){
        data.resize(size);
    }

    lc->release(ino);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    #if DB
    std::cout << "write:" << ino  << std::endl;
    std::cout.flush();
    #endif

    lc->acquire(ino);

    int r = OK;
    std::string buf;
    bytes_written = 0;
    r = ec->get(ino, buf);

    std::string temp(size, 0);
    for (int i=0;i<size;i++){
        temp[i] = data[i];
    }

    temp = temp.substr(0, size);
    if (buf.size() < off){
        bytes_written += off - buf.size();
        buf.append(off - buf.size(), 0);
        buf += temp;
    }else{
        buf.replace(off, size, temp);
    }
    
    bytes_written += size;
    ec->put(ino, buf);

    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    
    int r = OK;
    std::string buf;
    lc->acquire(parent);

    r = ec->get(parent, buf);

    int pos = 0;
    while(pos < buf.size()){
        const char * t = buf.c_str()+pos;
        if (strcmp(t, name) == 0){
            uint32_t ino = *(uint32_t *)(buf.c_str() + pos + strlen(t) + 1);
            if (isdir(ino)){
                lc->release(parent);
                return EXIST;
            }
            buf.erase(pos, strlen(t) + 1 + sizeof(uint32_t));
            ec->put(parent, buf);
            lc->acquire(ino);
            ec->remove(ino);
            lc->release(ino);
            break;
        }
        pos += strlen(t) + 1 + sizeof(uint32_t);
    }
    lc->release(parent);
    return r;
}


int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino)
{
    
    int r = OK;
    inum inode;
    std::string buf;

    lc->acquire(parent);
    bool found = false;
    lookup(parent, name, found, inode);
    if (found == true)
    {
        lc->release(parent);
        return EXIST;
    }
    ec->create(extent_protocol::T_SYMLK, ino);
    
    r = ec->get(parent, buf);
    buf.insert(buf.size(), name);
    buf.resize(buf.size() + 5, 0);
    *(uint32_t *)(buf.c_str()+buf.size()-4) = ino;
    
    ec->put(parent, buf);
    ec->put(ino, link);

    lc->release(parent);
    return r;
}

int yfs_client::readlink(inum ino, std::string &result)
{
    int r = OK;
    ec->get(ino, result);
    return r;
}
