#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <namenode.pb.h>
#include "namenode.h"
#include "hrpc.h"
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

using namespace std;
using namespace google::protobuf::io;

NameNode namenode;

int ReadHeader(int fd) {
  char buf[7];

  if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
    fprintf(stderr, "%s:%d Read header failed: %s\n", __func__, __LINE__, strerror(errno));
    fflush(stderr);
    return -1;
  }
  if (buf[0] != 'h' || buf[1] != 'r' || buf[2] != 'p' || buf[3] != 'c') {
    fprintf(stderr, "%s:%d Bad header\n", __func__, __LINE__);
    fflush(stderr);
    return -1;
  }
  if (buf[4] != 9) {
    fprintf(stderr, "%s:%d Unsupported version %d\n", __func__, __LINE__, buf[4]);
    fflush(stderr);
    return -1;
  }
  if (buf[5] != 0) {
    fprintf(stderr, "%s:%d Unsupported service class %d\n", __func__, __LINE__, buf[5]);
    fflush(stderr);
    return -1;
  }
  if (buf[6] != 0) {
    fprintf(stderr, "%s:%d Unsupported auth protocol %d\n", __func__, __LINE__, buf[6]);
    fflush(stderr);
    return -1;
  }
  return 0;
}

int HandleRpc(int fd) {
  RpcRequestHeaderProto rpc_header;
  RequestHeaderProto header;
  string buf;
  if (!HrpcRead(fd, rpc_header, buf)) {
    fprintf(stderr, "%s:%d read rpc request failed\n", __func__, __LINE__);
    fflush(stderr);
    return -1;
  }

  if (rpc_header.callid() < 0)
    return 0;

  if (ReadDelimited(buf, header) != 0) {
    fprintf(stderr, "%s:%d read request header failed\n", __func__, __LINE__);
    fflush(stderr);
    return -1;
  }

#define TRY(name) \
  if (strcasecmp(header.methodname().c_str(), #name) == 0) { \
    name##RequestProto req; \
    if (ReadDelimited(buf, req) != 0) { \
      fprintf(stderr, "%s:%d %s read request param failed\n", __func__, __LINE__, #name); \
      fflush(stderr); \
      return -1; \
    } \
    name##ResponseProto resp; \
    try { \
      namenode.PB##name(req, resp); \
      RpcResponseHeaderProto rpc_resp; \
      rpc_resp.set_callid(rpc_header.callid()); \
      rpc_resp.set_status(RpcResponseHeaderProto_RpcStatusProto_SUCCESS); \
      rpc_resp.set_serveripcversionnum(9); \
      rpc_resp.set_clientid(rpc_header.clientid()); \
      rpc_resp.set_retrycount(rpc_header.retrycount()); \
      HrpcWrite(fd, rpc_resp, resp); \
    } catch (HdfsException &e) { \
      RpcResponseHeaderProto rpc_resp; \
      rpc_resp.set_callid(rpc_header.callid()); \
      rpc_resp.set_status(RpcResponseHeaderProto_RpcStatusProto_ERROR); \
      rpc_resp.set_serveripcversionnum(9); \
      rpc_resp.set_clientid(rpc_header.clientid()); \
      rpc_resp.set_retrycount(rpc_header.retrycount()); \
      rpc_resp.set_errormsg(e.what()); \
      HrpcWrite(fd, rpc_resp); \
    } \
    return 0; \
  }
  TRY(GetFileInfo)
  TRY(GetListing)
  TRY(GetBlockLocations)
  TRY(RegisterDatanode)
  TRY(GetServerDefaults)
  TRY(Create)
  TRY(Complete)
  TRY(AddBlock)
  TRY(RenewLease)
  TRY(Rename)
  TRY(Delete)
  TRY(Mkdirs)
  TRY(GetFsStats)
  TRY(SetSafeMode)
  TRY(GetDatanodeReport)
  TRY(DatanodeHeartbeat)

  fprintf(stderr, "%s:%d Unknown namenode method %s\n", __func__, __LINE__, header.methodname().c_str());
  fflush(stderr);
  RpcResponseHeaderProto rpc_resp;
  rpc_resp.set_callid(rpc_header.callid());
  rpc_resp.set_status(RpcResponseHeaderProto_RpcStatusProto_ERROR);
  rpc_resp.set_serveripcversionnum(9);
  rpc_resp.set_clientid(rpc_header.clientid());
  rpc_resp.set_retrycount(rpc_header.retrycount());
  rpc_resp.set_errordetail(RpcResponseHeaderProto_RpcErrorCodeProto_ERROR_NO_SUCH_METHOD);
  rpc_resp.set_errormsg("Method not supported");
  HrpcWrite(fd, rpc_resp);

  return -1;
}

void *worker(void *_arg) {
  int clientfd = (int) (uintptr_t) _arg;

  if (ReadHeader(clientfd) != 0) {
    close(clientfd);
    return NULL;
  }

  for (;;) {
    if (HandleRpc(clientfd) != 0)
      break;
  }

  close(clientfd);
  return NULL;
}

bool NameNode::RecursiveLookup(const string &path, yfs_client::inum &ino, yfs_client::inum &last) {
  size_t pos = 1, lastpos = 1;
  bool found;
  if (path[0] != '/') {
    fprintf(stderr, "%s:%d Only absolute path allowed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  last = 1;
  ino = 1;
  while ((pos = path.find('/', pos)) != string::npos) {
    if (pos != lastpos) {
      string component = path.substr(lastpos, pos - lastpos);
      last = ino;
      if (yfs->lookup(ino, component.c_str(), found, ino) != yfs_client::OK || !found) {
        fprintf(stderr, "%s:%d Lookup %s in %llu failed\n", __func__, __LINE__, component.c_str(), ino); fflush(stderr);
        return false;
      }
    }
    pos++;
    lastpos = pos;
  }

  if (lastpos != path.size()) {
    last = ino;
    string component = path.substr(lastpos);
    if (yfs->lookup(ino, component.c_str(), found, ino) != yfs_client::OK || !found) {
      fprintf(stderr, "%s:%d Lookup %s in %llu failed\n", __func__, __LINE__, component.c_str(), ino); fflush(stderr);
      return false;
    }
  }

  return true;
}

bool NameNode::RecursiveLookup(const string &path, yfs_client::inum &ino) {
  yfs_client::inum last;
  return RecursiveLookup(path, ino, last);
}

bool NameNode::RecursiveLookupParent(const string &path, yfs_client::inum &ino) {
  string _path = path;
  while (_path.size() != 0 && _path[_path.size() - 1] == '/')
    _path.resize(_path.size() - 1);
  if (_path.rfind('/') != string::npos)
    _path = _path.substr(0, _path.rfind('/') + 1);
  return RecursiveLookup(_path, ino);
}

bool operator<(const DatanodeIDProto &a, const DatanodeIDProto &b) {
  if (a.ipaddr() < b.ipaddr())
    return true;
  if (a.ipaddr() > b.ipaddr())
    return false;
  if (a.hostname() < b.hostname())
    return true;
  if (a.hostname() > b.hostname())
    return false;
  if (a.xferport() < b.xferport())
    return true;
  if (a.xferport() > b.xferport())
    return false;
  return false;
}

bool operator==(const DatanodeIDProto &a, const DatanodeIDProto &b) {
  if (a.ipaddr() != b.ipaddr())
    return false;
  if (a.hostname() != b.hostname())
    return false;
  if (a.xferport() != b.xferport())
    return false;
  return true;
}

bool NameNode::ReplicateBlock(blockid_t bid, DatanodeIDProto from, DatanodeIDProto to) {
  unique_ptr<FileInputStream> pfis;
  unique_ptr<FileOutputStream> pfos;
  if (!Connect(from.ipaddr(), from.xferport(), pfis, pfos)) {
    fprintf(stderr, "%s:%d Connect to %s:%d failed\n", __func__, __LINE__, from.hostname().c_str(), from.xferport()); fflush(stderr);
    return false;
  }
  CodedOutputStream cos(&*pfos);
  uint8_t hdr[3] = { 0, 28, 86 };
  OpTransferBlockProto req;
  req.mutable_header()->mutable_baseheader()->mutable_block()->set_poolid("yfs");
  req.mutable_header()->mutable_baseheader()->mutable_block()->set_blockid(bid);
  req.mutable_header()->mutable_baseheader()->mutable_block()->set_generationstamp(0);
  req.mutable_header()->mutable_baseheader()->mutable_block()->set_numbytes(BLOCK_SIZE);
  req.mutable_header()->set_clientname("");
  req.add_targets()->mutable_id()->CopyFrom(to);
  req.add_targetstoragetypes(RAM_DISK);
  cos.WriteRaw(hdr, sizeof(hdr));
  cos.WriteVarint32(req.ByteSize());
  req.SerializeWithCachedSizes(&cos);
  cos.Trim();
  pfos->Flush();
  if (cos.HadError()) {
    fprintf(stderr, "%s:%d send transfer request failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  BlockOpResponseProto resp;
  CodedInputStream cis(&*pfis);
  auto limit = cis.ReadLengthAndPushLimit();
  if (!resp.ParseFromCodedStream(&cis)) {
    fprintf(stderr, "%s:%d bad response\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (!cis.CheckEntireMessageConsumedAndPopLimit(limit)) {
    fprintf(stderr, "%s:%d bad response\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (resp.status() != SUCCESS) {
    fprintf(stderr, "%s:%d transfer block from %s to %s failed\n", __func__, __LINE__, from.hostname().c_str(), to.hostname().c_str()); fflush(stderr);
    return false;
  }
  return true;
}

// Translators

bool NameNode::PBGetFileInfoFromInum(yfs_client::inum ino, HdfsFileStatusProto &info) {
  info.set_filetype(HdfsFileStatusProto_FileType_IS_FILE);
  info.set_path("");
  info.set_length(0);
  info.set_owner("cse");
  info.set_group("supergroup");
  info.set_blocksize(BLOCK_SIZE);
  if (Isfile(ino)) {
    yfs_client::fileinfo yfs_info;
    if (!Getfile(ino, yfs_info)) {
      fprintf(stderr, "%s:%d Getfile(%llu) failed\n", __func__, __LINE__, ino); fflush(stderr);
      return false;
    }
    info.set_length(yfs_info.size);
    info.mutable_permission()->set_perm(0666);
    info.set_modification_time(((uint64_t) yfs_info.mtime) * 1000);
    info.set_access_time(((uint64_t) yfs_info.atime) * 1000);
    return true;
  } else if (Isdir(ino)) {
    yfs_client::dirinfo yfs_info;
    if (!Getdir(ino, yfs_info)) {
      fprintf(stderr, "%s:%d Getdir(%llu) failed\n", __func__, __LINE__, ino); fflush(stderr);
      return false;
    }
    info.set_filetype(HdfsFileStatusProto_FileType_IS_DIR);
    info.mutable_permission()->set_perm(0777);
    info.set_modification_time(((uint64_t) yfs_info.mtime) * 1000);
    info.set_access_time(((uint64_t) yfs_info.atime) * 1000);
    return true;
  }

  return false;
}

void NameNode::PBGetFileInfo(const GetFileInfoRequestProto &req, GetFileInfoResponseProto &resp) {
  yfs_client::inum ino;
  if (!RecursiveLookup(req.src(), ino))
    return;
  if (!PBGetFileInfoFromInum(ino, *resp.mutable_fs()))
    resp.clear_fs();
}

void NameNode::PBGetListing(const GetListingRequestProto &req, GetListingResponseProto &resp) {
  yfs_client::inum ino;
  if (!RecursiveLookup(req.src(), ino))
    return;
  string start_after(req.startafter());
  list<yfs_client::dirent> dir;
  if (yfs->readdir(ino, dir) != yfs_client::OK)
    throw HdfsException("read directory failed");
  auto it = dir.begin();
  if (start_after.size() != 0) {
    while (it != dir.end() && it->name != start_after)
      it++;
  }
  for (; it != dir.end(); it++) {
    if (!PBGetFileInfoFromInum(it->inum, *resp.mutable_dirlist()->add_partiallisting()))
      throw HdfsException("get dirent info failed");
    resp.mutable_dirlist()->mutable_partiallisting()->rbegin()->set_path(it->name);
    if (req.needlocation() && yfs->isfile(it->inum)) {
      yfs_client::fileinfo info;
      if (yfs->getfile(it->inum, info) != yfs_client::OK)
        return;
      list<LocatedBlock> blocks = GetBlockLocations(it->inum);
      LocatedBlocksProto &locations = *resp.mutable_dirlist()->mutable_partiallisting()->rbegin()->mutable_locations();
      locations.set_filelength(info.size);
      locations.set_underconstruction(false);
      locations.set_islastblockcomplete(true);
      int i = 0;
      for (auto it = blocks.begin(); it != blocks.end(); it++) {
        LocatedBlockProto *block = locations.add_blocks();
        if (!ConvertLocatedBlock(*it, *block))
          throw HdfsException("Failed to convert located block");
        if (next(it) == blocks.end())
          locations.mutable_lastblock()->CopyFrom(*block);
        i++;
      }
    }
  }
  resp.mutable_dirlist()->set_remainingentries(0);
}

bool NameNode::ConvertLocatedBlock(const LocatedBlock &src, LocatedBlockProto &dst) {
  dst.mutable_b()->set_poolid("yfs");
  dst.mutable_b()->set_blockid(src.block_id);
  dst.mutable_b()->set_generationstamp(0);
  dst.mutable_b()->set_numbytes(src.size);
  dst.mutable_blocktoken()->set_identifier("");
  dst.mutable_blocktoken()->set_password("");
  dst.mutable_blocktoken()->set_kind("");
  dst.mutable_blocktoken()->set_service("");
  dst.set_offset(src.offset);
  for (auto it = src.locs.begin(); it != src.locs.end(); it++) {
    DatanodeInfoProto *loc = dst.add_locs();
    loc->mutable_id()->CopyFrom(*it);
    loc->set_location("/default-rack");
    dst.add_iscached(false);
    dst.add_storagetypes(StorageTypeProto::RAM_DISK);
    dst.add_storageids(it->hostname());
  }
  dst.set_corrupt(false);
  dst.mutable_blocktoken();
  return true;
}

void NameNode::PBGetBlockLocations(const GetBlockLocationsRequestProto &req, GetBlockLocationsResponseProto &resp) {
  yfs_client::inum ino;
  if (!RecursiveLookup(req.src(), ino))
    return;
  if (!yfs->isfile(ino))
    return;
  yfs_client::fileinfo info;
  if (yfs->getfile(ino, info) != yfs_client::OK)
    return;
  list<LocatedBlock> blocks = GetBlockLocations(ino);
  LocatedBlocksProto &locations = *resp.mutable_locations();
  locations.set_filelength(info.size);
  locations.set_underconstruction(false);
  locations.set_islastblockcomplete(true);
  int i = 0;
  for (auto it = blocks.begin(); it != blocks.end(); it++) {
    LocatedBlockProto *block = locations.add_blocks();
    if (!ConvertLocatedBlock(*it, *block))
      throw HdfsException("Failed to convert located block");
    if (next(it) == blocks.end())
      locations.mutable_lastblock()->CopyFrom(*block);
    i++;
  }
}

struct DatanodeRegistration {
  NameNode *namenode;
  DatanodeIDProto id;
};

void *NameNode::_RegisterDatanode(void *arg) {
  DatanodeRegistration *registration = (DatanodeRegistration *) arg;
  registration->namenode->RegisterDatanode(registration->id);
  delete registration;
  return NULL;
}

void NameNode::PBRegisterDatanode(const RegisterDatanodeRequestProto &req, RegisterDatanodeResponseProto &resp) {
  if (req.registration().master())
    master_datanode = req.registration().datanodeid();
  pthread_t thread;
  DatanodeRegistration *registration = new DatanodeRegistration();
  registration->namenode = this;
  registration->id.CopyFrom(req.registration().datanodeid());
  if (pthread_create(&thread, NULL, NameNode::_RegisterDatanode, registration) != 0)
    throw HdfsException("Failed to register datanode");
  resp.mutable_registration()->CopyFrom(req.registration());
}

void NameNode::PBGetServerDefaults(const GetServerDefaultsRequestProto &req, GetServerDefaultsResponseProto &resp) {
  FsServerDefaultsProto &defaults = *resp.mutable_serverdefaults();
  defaults.set_blocksize(BLOCK_SIZE);
  defaults.set_bytesperchecksum(1);
  defaults.set_writepacketsize(BLOCK_SIZE);
  defaults.set_replication(1);
  defaults.set_filebuffersize(4096);
  defaults.set_checksumtype(CHECKSUM_NULL);
}

void NameNode::PBCreate(const CreateRequestProto &req, CreateResponseProto &resp) {
  size_t pos = 1, lastpos = 1;
  bool found;
  string path = req.src();
  if (path[0] != '/')
    throw HdfsException("Not absolute path");
  yfs_client::inum ino = 1;
  while ((pos = path.find('/', pos)) != string::npos) {
    if (pos != lastpos) {
      string component = path.substr(lastpos, pos - lastpos);
      if (yfs->lookup(ino, component.c_str(), found, ino) != yfs_client::OK)
        throw HdfsException("Traverse failed");
      if (!found) {
        if (req.createparent()) {
          if (!Mkdir(ino, component.c_str(), 0777, ino))
            throw HdfsException("Create parent failed");
        } else
          throw HdfsException("Parent not exists");
      }
    }
    pos++;
    lastpos = pos;
  }

  if (lastpos != path.size()) {
    string component = path.substr(lastpos);
    if (!Create(ino, component.c_str(), 0666, ino))
      throw HdfsException("Create file failed");
    pendingWrite.insert(make_pair(ino, 0));
    if (!PBGetFileInfoFromInum(ino, *resp.mutable_fs()))
      resp.clear_fs();
  } else
    throw HdfsException("No file name given");
}

void NameNode::PBComplete(const CompleteRequestProto &req, CompleteResponseProto &resp) {
  yfs_client::inum ino;
  if (!RecursiveLookup(req.src(), ino)) {
    resp.set_result(false);
    return;
  }
  if (pendingWrite.count(ino) == 0)
    throw HdfsException("No such pending write");
  if (req.has_last()) {
    pendingWrite[ino] -= BLOCK_SIZE;
    pendingWrite[ino] += req.last().numbytes();
  }
  uint32_t new_size = pendingWrite[ino];
  pendingWrite.erase(ino);
  bool r = Complete(ino, new_size);
  if (!r) {
    pendingWrite.insert(make_pair(ino, new_size));
    throw HdfsException("Complete pending write failed");
  }
  resp.set_result(r);
}

void NameNode::PBAddBlock(const AddBlockRequestProto &req, AddBlockResponseProto &resp) {
  yfs_client::inum ino;
  if (!RecursiveLookup(req.src(), ino))
    return;
  if (pendingWrite.count(ino) == 0)
    throw HdfsException("File not locked");
  LocatedBlock new_block = AppendBlock(ino);
  if (!ConvertLocatedBlock(new_block, *resp.mutable_block()))
    throw HdfsException("Convert LocatedBlock failed");
  pendingWrite[ino] += BLOCK_SIZE;
}

void NameNode::PBRenewLease(const RenewLeaseRequestProto &req, RenewLeaseResponseProto &resp) {
}

void NameNode::PBRename(const RenameRequestProto &req, RenameResponseProto &resp) {
  yfs_client::inum src_dir_ino, dst_dir_ino;
  string src_name, dst_name;
  if (!RecursiveLookupParent(req.src(), src_dir_ino)) {
    resp.set_result(false);
    return;
  }
  if (!RecursiveLookupParent(req.dst(), dst_dir_ino)) {
    resp.set_result(false);
    return;
  }
  src_name = req.src().substr(req.src().rfind('/') + 1);
  dst_name = req.dst().substr(req.dst().rfind('/') + 1);
  resp.set_result(Rename(src_dir_ino, src_name, dst_dir_ino, dst_name));
}

void NameNode::DualLock(lock_protocol::lockid_t a, lock_protocol::lockid_t b) {
  if (a < b) {
    lc->acquire(a);
    lc->acquire(b);
  } else if (a > b) {
    lc->acquire(b);
    lc->acquire(a);
  } else
    lc->acquire(a);
}

void NameNode::DualUnlock(lock_protocol::lockid_t a, lock_protocol::lockid_t b) {
  if (a == b)
    lc->release(a);
  else {
    lc->release(a);
    lc->release(b);
  }
}

bool NameNode::RecursiveDelete(yfs_client::inum ino) {
  list<yfs_client::dirent> dir;
  if (!Isdir(ino))
    return true;
  if (!Readdir(ino, dir)) {
    fprintf(stderr, "%s:%d Readdir(%llu) failed\n", __func__, __LINE__, ino); fflush(stderr);
    return false;
  }
  for (auto it = dir.begin(); it != dir.end(); it++) {
    lc->acquire(it->inum);
    if (!RecursiveDelete(it->inum)) {
      lc->release(it->inum);
      fprintf(stderr, "%s:%d recursively delete %s failed\n", __func__, __LINE__, it->name.c_str()); fflush(stderr);
      return false;
    }
    if (!Unlink(ino, it->name, it->inum)) {
      lc->release(it->inum);
      fprintf(stderr, "%s:%d delete %s failed\n", __func__, __LINE__, it->name.c_str()); fflush(stderr);
      return false;
    }
    lc->release(it->inum);
  }
  return true;
}

void NameNode::PBDelete(const DeleteRequestProto &req, DeleteResponseProto &resp) {
  yfs_client::inum ino, parent;
  if (!RecursiveLookup(req.src(), ino, parent)) {
    resp.set_result(false);
    return;
  }
  DualLock(ino, parent);
  if (req.recursive()) {
    if (!RecursiveDelete(ino)) {
      resp.set_result(false);
      DualUnlock(ino, parent);
      return;
    }
  }
  if (Isdir(ino)) {
    list<yfs_client::dirent> dir;
    if (!Readdir(ino, dir)) {
      resp.set_result(false);
      DualUnlock(ino, parent);
      return;
    }
    if (dir.size() != 0) {
      resp.set_result(false);
      DualUnlock(ino, parent);
      return;
    }
  }
  if (!Unlink(parent, req.src().substr(req.src().rfind('/') + 1), ino)) {
    resp.set_result(false);
    DualUnlock(ino, parent);
    return;
  }
  resp.set_result(true);
  DualUnlock(ino, parent);
}

void NameNode::PBMkdirs(const MkdirsRequestProto &req, MkdirsResponseProto &resp) {
  size_t pos = 1, lastpos = 1;
  bool found;
  string path = req.src();
  if (path[0] != '/')
    throw HdfsException("Not absolute path");
  yfs_client::inum ino = 1;
  while ((pos = path.find('/', pos)) != string::npos) {
    if (pos != lastpos) {
      string component = path.substr(lastpos, pos - lastpos);
      if (yfs->lookup(ino, component.c_str(), found, ino) != yfs_client::OK)
        throw HdfsException("Traverse failed");
      if (!found) {
        if (req.createparent()) {
          if (!Mkdir(ino, component.c_str(), 0777, ino))
            throw HdfsException("Create parent failed");
        } else
          throw HdfsException("Parent not exists");
      }
    }
    pos++;
    lastpos = pos;
  }

  if (lastpos != path.size()) {
    string component = path.substr(lastpos);
    if (!Mkdir(ino, component.c_str(), 0777, ino))
      throw HdfsException("Mkdirs failed");
  } else
    throw HdfsException("No file name given");

  resp.set_result(true);
}

void NameNode::PBGetFsStats(const GetFsStatsRequestProto &req, GetFsStatsResponseProto &resp) {
  resp.set_capacity(0);
  resp.set_used(0);
  resp.set_remaining(0);
  resp.set_under_replicated(0);
  resp.set_corrupt_blocks(0);
  resp.set_missing_blocks(0);
}

void NameNode::PBSetSafeMode(const SetSafeModeRequestProto &req, SetSafeModeResponseProto &resp) {
  resp.set_result(false);
}

void NameNode::PBGetDatanodeReport(const GetDatanodeReportRequestProto &req, GetDatanodeReportResponseProto &resp) {
  switch (req.type()) {
  case ALL:
  case LIVE: {
    list<DatanodeIDProto> datanodes = GetDatanodes();
    for (auto it = datanodes.begin(); it != datanodes.end(); it++)
      resp.add_di()->mutable_id()->CopyFrom(*it);
    break; }
  case DEAD:
  case DECOMMISSIONING:
    break;
  }
}

void NameNode::PBDatanodeHeartbeat(const DatanodeHeartbeatRequestProto &req, DatanodeHeartbeatResponseProto &resp) {
  DatanodeHeartbeat(req.id());
}

// Main

int main(int argc, char *argv[]) {
  if(argc != 3){
    fprintf(stderr, "Usage: %s <extent_server> <lock_server>\n", argv[0]);
    fflush(stderr);
    exit(1);
  }

  int acceptfd = socket(AF_INET, SOCK_STREAM, 0);
  if (acceptfd < 0) {
    fprintf(stderr, "Create listen socket failed\n");
    fflush(stderr);
    return 1;
  }

  int yes = 1;
  if (setsockopt(acceptfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
    fprintf(stderr, "Enable SO_REUSEADDR failed: %s\n", strerror(errno));
    fflush(stderr);
    return 1;
  }

  struct sockaddr_in bindaddr;
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_port = htons(9000);
  bindaddr.sin_addr.s_addr = 0;
  if (bind(acceptfd, (struct sockaddr *) &bindaddr, sizeof(bindaddr)) != 0) {
    fprintf(stderr, "Bind listen socket failed: %s\n", strerror(errno));
    fflush(stderr);
    return 1;
  }
  if (listen(acceptfd, 1) != 0) {
    fprintf(stderr, "Listen on :9000 failed: %s\n", strerror(errno));
    fflush(stderr);
    return 1;
  }

  namenode.init(argv[1], argv[2]);

  for (;;) {
    int clientfd = accept(acceptfd, NULL, 0);
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, (void *) (uintptr_t) clientfd) != 0) {
      fprintf(stderr, "Create handler thread failed: %s\n", strerror(errno));
      fflush(stderr);
    }
  }

  return 0;
}
