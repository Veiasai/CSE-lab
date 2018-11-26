#ifndef NAMENODE_H_
#define NAMENODE_H_

#include <namenode.pb.h>
#include <string>
#include <list>
#include "yfs_client.h"
#include <stdexcept>
#include <set>
#include <unordered_map>

class extent_client;
class lock_client;
class yfs_client;

class HdfsException : public std::runtime_error {
public:
  HdfsException() : runtime_error("") {}
  HdfsException(const char *what) : runtime_error(what) {}
};

bool operator<(const DatanodeIDProto &, const DatanodeIDProto &);
bool operator==(const DatanodeIDProto &, const DatanodeIDProto &);

class NameNode {
  struct LocatedBlock {
    blockid_t block_id;
    uint64_t offset, size;
    std::list<DatanodeIDProto> locs;
    LocatedBlock(blockid_t block_id,
                 uint64_t offset,
                 uint64_t size,
                 const DatanodeIDProto &loc) :
        block_id(block_id),
        offset(offset),
        size(size),
        locs(1, loc) {}
    LocatedBlock(blockid_t block_id,
                 uint64_t offset,
                 uint64_t size,
                 const std::list<DatanodeIDProto> &locs) :
        block_id(block_id),
        offset(offset),
        size(size),
        locs(locs) {}
  };

private:
  extent_client *ec;
  lock_client_cache *lc;
  yfs_client *yfs;
  DatanodeIDProto master_datanode;
  std::map<yfs_client::inum, uint32_t> pendingWrite;

  /* Add your member variables/functions here */
private:
  void GetFileInfo();
  bool RecursiveLookup(const std::string &path, yfs_client::inum &ino, yfs_client::inum &last);
  bool RecursiveLookup(const std::string &path, yfs_client::inum &ino);
  bool RecursiveLookupParent(const std::string &path, yfs_client::inum &ino);
  bool RecursiveDelete(yfs_client::inum ino);
  bool ConvertLocatedBlock(const LocatedBlock &src, LocatedBlockProto &dst);
  std::list<LocatedBlock> GetBlockLocations(yfs_client::inum ino);
  bool Complete(yfs_client::inum ino, uint32_t new_size);
  LocatedBlock AppendBlock(yfs_client::inum ino);
  bool Rename(yfs_client::inum src_dir_ino, std::string src_name, yfs_client::inum dst_dir_ino, std::string dst_name);
  bool Mkdir(yfs_client::inum parent, std::string name, mode_t mode, yfs_client::inum &ino_out);
  bool Create(yfs_client::inum parent, std::string name, mode_t mode, yfs_client::inum &ino_out);
  bool Isfile(yfs_client::inum ino);
  bool Isdir(yfs_client::inum ino);
  bool Readlink(yfs_client::inum ino, std::string &dest);
  bool Getfile(yfs_client::inum, yfs_client::fileinfo &);
  bool Getdir(yfs_client::inum, yfs_client::dirinfo &);
  void DualLock(lock_protocol::lockid_t a, lock_protocol::lockid_t b);
  void DualUnlock(lock_protocol::lockid_t a, lock_protocol::lockid_t b);
  bool Readdir(yfs_client::inum, std::list<yfs_client::dirent> &);
  bool Unlink(yfs_client::inum parent, std::string name, yfs_client::inum ino);
  static void *_RegisterDatanode(void *arg);
  void RegisterDatanode(DatanodeIDProto id);
  void DatanodeHeartbeat(DatanodeIDProto id);
  std::list<DatanodeIDProto> GetDatanodes();
  static bool ReplicateBlock(blockid_t bid, DatanodeIDProto from, DatanodeIDProto to);
public:
  void init(const std::string &extent_dst, const std::string &lock_dst);
  bool PBGetFileInfoFromInum(yfs_client::inum ino, HdfsFileStatusProto &info);
  void PBGetFileInfo(const GetFileInfoRequestProto &req, GetFileInfoResponseProto &resp);
  void PBGetListing(const GetListingRequestProto &req, GetListingResponseProto &resp);
  void PBGetBlockLocations(const GetBlockLocationsRequestProto &req, GetBlockLocationsResponseProto &resp);
  void PBRegisterDatanode(const RegisterDatanodeRequestProto &req, RegisterDatanodeResponseProto &resp);
  void PBGetServerDefaults(const GetServerDefaultsRequestProto &req, GetServerDefaultsResponseProto &resp);
  void PBCreate(const CreateRequestProto &req, CreateResponseProto &resp);
  void PBComplete(const CompleteRequestProto &req, CompleteResponseProto &resp);
  void PBAddBlock(const AddBlockRequestProto &req, AddBlockResponseProto &resp);
  void PBRenewLease(const RenewLeaseRequestProto &req, RenewLeaseResponseProto &resp);
  void PBRename(const RenameRequestProto &req, RenameResponseProto &resp);
  void PBDelete(const DeleteRequestProto &req, DeleteResponseProto &resp);
  void PBMkdirs(const MkdirsRequestProto &req, MkdirsResponseProto &resp);
  void PBGetFsStats(const GetFsStatsRequestProto &req, GetFsStatsResponseProto &resp);
  void PBSetSafeMode(const SetSafeModeRequestProto &req, SetSafeModeResponseProto &resp);
  void PBGetDatanodeReport(const GetDatanodeReportRequestProto &req, GetDatanodeReportResponseProto &resp);
  void PBDatanodeHeartbeat(const DatanodeHeartbeatRequestProto &req, DatanodeHeartbeatResponseProto &resp);
};

#endif
