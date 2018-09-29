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

#define FILENAME_MAX 60

yfs_client::yfs_client()
{
    ec = new extent_client();
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }else if (a.type == extent_protocol::T_SYMLK) {
        printf("isfile: %lld is a symlink\n", inum);
        return false;
    } 
    printf("isfile: %lld is a dir\n", inum);
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLK) {
        printf("isfile: %lld is a symlink\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a symlink\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a dir\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
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
    return r;
}

int
yfs_client::getsymlink(inum inum, symlinkinfo &sin1)
{
    int r = OK;
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
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
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
    if (!isfile(ino))
        return EXIST;
    int r = OK;
    std::string buf;
    r = ec->get(ino, buf);
    buf.resize(size);
    ec->put(ino, buf);
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    printf("create name: %s \n", name);
    int r = OK;
    bool found = false;
    lookup(parent, name, found, ino_out);
    if (found == true)
    {
        return EXIST;
    }
    ec->create(extent_protocol::T_FILE, ino_out);
    std::string buf;
    r = ec->get(parent, buf);
    buf.insert(buf.size(), name);
    buf.resize(buf.size() + FILENAME_MAX + 4 - strlen(name), 0);
    *(uint32_t *)(buf.c_str()+buf.size()-4) = ino_out;

    std::cout << "create inum " << ino_out << " buf size " << buf.size() << '\n';
    
    ec->put(parent, buf);
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    bool found = false;
    lookup(parent, name, found, ino_out);
    if (!isdir(parent))
        return EXIST;
    if (found == true)
    {
        return EXIST;
    }
    printf("make dir name: %s \n", name);
    int r = OK;
    r = ec->create(extent_protocol::T_DIR, ino_out);
    std::string buf;
    r = ec->get(parent, buf);
    buf.insert(buf.size(), name);
    buf.resize(buf.size() + FILENAME_MAX + 4 - strlen(name), 0);
    *(uint32_t *)(buf.c_str()+buf.size()-4) = ino_out;

    std::cout << "make dir inum" << ino_out << '\n'; 
    ec->put(parent, buf);

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    std::string buf;
    r = ec->get(parent, buf);
    
    if (!isdir(parent))
    {
        return EXIST;
    }
    int pos = 0;
    while(pos < buf.size()){
        if (strcmp(buf.c_str()+pos, name) == 0){
            ino_out = *(uint32_t *)(buf.c_str() + pos + FILENAME_MAX);
            found = true;
            std::cout << "lookup ok, ino: " << ino_out << "\n";
            return r;
        }
        pos += FILENAME_MAX + sizeof(uint);
    }
    found = false;
    std::cout << "lookup fail" << "name: " << name << "\n";
    std::cout << "parent: " << parent << "\n";
            
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    if (!isdir(dir)){
        return EXIST;
    }
    int r = OK;
    std::string buf;
    r = ec->get(dir, buf);
    int pos = 0;
    while(pos < buf.size()){
        struct dirent temp;
        temp.name = buf.substr(pos, FILENAME_MAX);
        temp.inum = *(uint32_t *)(buf.c_str() + pos + FILENAME_MAX);
        list.push_back(temp);
        pos += FILENAME_MAX + sizeof(uint32_t);
        std::cout << "readdir name: " << temp.name << "\n";
        std::cout << "pos:" << pos << "\n";
    }
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    if (!isfile(ino)){
        return EXIST;
    }
    int r = OK;
    r = ec->get(ino, data);

    std::cout << "read file size: " << size << " data size: " << data.size() << "\n";
    std::cout << "read file off: " << off << "\n";

    if (off < data.size()){
        data=data.substr(off);
    }else{
        data="";
    }
    if (data.size() > size){
        data.resize(size);
    }

    std::cout << "result size: " << data.size() << "\n";
    /*
     * your code goes here.
     * note: read using ec->get().
     */

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    std::string buf;
    bytes_written = 0;
    r = ec->get(ino, buf);

    std::cout << "write file size: " << size << " buf size" << buf.size() << " off " << off << "\n";

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

    std::cout << "ino " << ino << " bytes written: " << bytes_written;
    std::cout << "result size: " << buf.size();
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    std::string buf;
    if (strlen(name) < 1)
        return EXIST;
    if (!isdir(parent)){
        return EXIST;
    }
    r = ec->get(parent, buf);

    std::cout << "unlink: " << name << " parent: " << parent << "\n";
    int pos = 0;
    while(pos < buf.size()){
        if (strcmp(buf.c_str()+pos, name) == 0){
            uint32_t ino = *(uint32_t *)(buf.c_str() + pos + FILENAME_MAX);
            if (isdir(ino))
                return EXIST;
            ec->remove(ino);
            buf.erase(pos, FILENAME_MAX + sizeof(uint32_t));
            ec->put(parent, buf);
            break;
        }
        pos += FILENAME_MAX + sizeof(uint32_t);
    }
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    return r;
}


int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino)
{
    int r = OK;
    bool found = false;
    inum inode;
    std::string buf;

    std::cout << "create link: " << link << " name: " << name << "\n";
    std::cout << "parent: " << parent << "\n";
    lookup(parent, name, found, inode);
    if (found == true)
    {
        return EXIST;
    }
    ec->create(extent_protocol::T_SYMLK, ino);
    
    r = ec->get(parent, buf);
    buf.insert(buf.size(), name);
    buf.resize(buf.size() + FILENAME_MAX + 4 - strlen(name), 0);
    *(uint32_t *)(buf.c_str()+buf.size()-4) = ino;
    
    ec->put(parent, buf);

    buf = link;

    ec->put(ino, buf);
    return r;
}

int yfs_client::readlink(inum ino, std::string &result)
{
    int r = OK;
    ec->get(ino, result);
    return r;
}
