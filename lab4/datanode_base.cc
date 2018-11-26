#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include "datanode.h"
#include <unistd.h>
#include <netdb.h>
#include "hrpc.h"
#include <arpa/inet.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "inode_manager.h"

#define PACKET_SIZE 16384

using namespace std;
using namespace google::protobuf::io;

DataNode datanode;

string DataNode::GetHostname() {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    fprintf(stderr, "%s:%d Get host name failed: %s\n", __func__, __LINE__, strerror(errno));
    fflush(stderr);
    return "";
  }
  return hostname;
}

string DataNode::GenerateUUID() {
  char buf[37];
  union {
    char bytes[16];
    struct {
      uint32_t a;
      uint16_t b, c, d;
      uint8_t e, f, g, h, i, j;
    } __attribute__((packed)) data;
  } rnd;
  for (size_t i = 0; i < sizeof(rnd.bytes); i++)
    rnd.bytes[i] = rand();
  snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
      rnd.data.a,
      rnd.data.b,
      rnd.data.c,
      rnd.data.d,
      rnd.data.e,
      rnd.data.f,
      rnd.data.g,
      rnd.data.h,
      rnd.data.i,
      rnd.data.j);
  return buf;
}

bool GetPrimaryIP(struct in_addr *addr) {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    fprintf(stderr, "%s:%d Get host name failed: %s\n", __func__, __LINE__, strerror(errno));
    fflush(stderr);
    return false;
  }
  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  struct addrinfo *ai = NULL;
  if (getaddrinfo(hostname, NULL, &hint, &ai) != 0) {
    fprintf(stderr, "%s:%d resolve self IP failed\n", __func__, __LINE__);
    fflush(stderr);
    return false;
  }
  *addr = ((struct sockaddr_in *) ai->ai_addr)->sin_addr;
  freeaddrinfo(ai);
  return true;
}

bool DataNode::ConnectToNN() {
  namenode_conn = socket(AF_INET, SOCK_STREAM, 0);
  if (namenode_conn == -1) {
    fprintf(stderr, "%s:%d create socket failed: %s\n", __func__, __LINE__, strerror(errno));
    return false;
  }
  if (connect(namenode_conn, (struct sockaddr *) &this->namenode_addr, sizeof(namenode_addr)) != 0) {
    fprintf(stderr, "%s:%d connect to namenode failed: %s\n", __func__, __LINE__, strerror(errno));
    close(namenode_conn);
    namenode_conn = -1;
    return false;
  }
  if (write(namenode_conn, "hrpc\x09\x00\x00", 7) != 7) {
    fprintf(stderr, "%s:%d write hrpc header failed: %s\n", __func__, __LINE__, strerror(errno));
    close(namenode_conn);
    namenode_conn = -1;
    return false;
  }
  return true;
}

bool DataNode::RegisterOnNamenode() {
  RpcRequestHeaderProto rpc_header;
  RequestHeaderProto header;
  RegisterDatanodeRequestProto req;
  rpc_header.set_callid(0);
  rpc_header.set_clientid(GenerateUUID());
  header.set_methodname("registerDatanode");
  header.set_declaringclassprotocolname("org.apache.hadoop.hdfs.server.protocol.DatanodeProtocol");
  header.set_clientprotocolversion(1);
  if (GetHostname() == "name")
    req.mutable_registration()->set_master(true);
  else
    req.mutable_registration()->set_master(false);
  req.mutable_registration()->set_allocated_datanodeid(&id);
  if (!HrpcWrite(namenode_conn, rpc_header, header, req)) {
    req.mutable_registration()->release_datanodeid();
    fprintf(stderr, "%s:%d send registration of %s to namenode failed\n", __func__, __LINE__, id.hostname().c_str()); fflush(stderr);
    return false;
  }
  req.mutable_registration()->release_datanodeid();

  // Read response
  RpcResponseHeaderProto rpc_resp;
  string buf;
  if (!HrpcRead(namenode_conn, rpc_resp, buf)) {
    fprintf(stderr, "%s:%d read response failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (rpc_resp.status() != RpcResponseHeaderProto_RpcStatusProto_SUCCESS) {
    fprintf(stderr, "%s:%d register on namenode failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  return true;
}

bool DataNode::SendHeartbeat() {
  RpcRequestHeaderProto rpc_header;
  RequestHeaderProto header;
  DatanodeHeartbeatRequestProto req;
  rpc_header.set_callid(0);
  rpc_header.set_clientid(GenerateUUID());
  header.set_methodname("datanodeHeartbeat");
  header.set_declaringclassprotocolname("yfs");
  header.set_clientprotocolversion(1);
  req.mutable_id()->CopyFrom(id);
  if (!HrpcWrite(namenode_conn, rpc_header, header, req)) {
    fprintf(stderr, "%s:%d write heartbeat request failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  // Read response
  RpcResponseHeaderProto rpc_resp;
  string buf;
  if (!HrpcRead(namenode_conn, rpc_resp, buf)) {
    fprintf(stderr, "%s:%d read heartbeat response failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (rpc_resp.status() != RpcResponseHeaderProto_RpcStatusProto_SUCCESS) {
    fprintf(stderr, "%s:%d heartbeat failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  return true;
}

static bool ReadOp(CodedInputStream &is, int &op) {
  uint8_t buf[3];
  if (!is.ReadRaw(buf, sizeof(buf))) {
    fprintf(stderr, "%s:%d read op failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (buf[0] != 0 || buf[1] != 28) {
    fprintf(stderr, "%s:%d datanode protocol version not supported\n", __func__, __LINE__); fflush(stderr);
    return false; // Wrong version
  }
  op = buf[2];
  return true;
}

bool WritePacket(CodedOutputStream &cos, FileOutputStream &fos, const PacketHeaderProto &header, const void *buf) {
  uint32_t plen = sizeof(plen) + header.datalen();
  plen = htonl(plen);
  cos.WriteRaw(&plen, sizeof(plen));
  uint16_t hlen = 25;
  hlen = htons(hlen);
  cos.WriteRaw(&hlen, sizeof(hlen));
  if (!header.SerializeToCodedStream(&cos)) {
    fprintf(stderr, "%s:%d write packet header failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  cos.WriteRaw(buf, header.datalen());
  cos.Trim();
  fos.Flush();
  return !cos.HadError();
}

bool WritePacket(CodedOutputStream &cos, FileOutputStream &fos, uint64_t offset_in_block, uint64_t seqno, bool last_in_block, uint64_t len, const void *buf) {
  PacketHeaderProto header;
  header.set_offsetinblock(offset_in_block);
  header.set_seqno(seqno);
  header.set_lastpacketinblock(last_in_block);
  header.set_datalen(len);
  return WritePacket(cos, fos, header, buf);
}

bool DataNode::_ReadBlock(CodedInputStream &is, CodedOutputStream &os, FileOutputStream &raw_os) {
  // Read request
  auto limit = is.ReadLengthAndPushLimit();
  OpReadBlockProto param;
  if (!param.ParseFromCodedStream(&is) || !is.CheckEntireMessageConsumedAndPopLimit(limit)) {
    BlockOpResponseProto resp;
    resp.set_status(ERROR);
    os.WriteVarint32(resp.ByteSize());
    resp.SerializeWithCachedSizes(&os);
    fprintf(stderr, "%s:%d invalid read block request\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  // Read block
  string block;
  if (!ReadBlock(param.header().baseheader().block().blockid(), param.offset(), param.len(), block)) {
    BlockOpResponseProto resp;
    resp.set_status(ERROR);
    os.WriteVarint32(resp.ByteSize());
    resp.SerializeWithCachedSizes(&os);
    fprintf(stderr, "%s:%d read block %lu failed\n", __func__, __LINE__, param.header().baseheader().block().blockid()); fflush(stderr);
    return false;
  }

  // Send back
  BlockOpResponseProto resp;
  resp.set_status(SUCCESS);
  resp.mutable_readopchecksuminfo()->mutable_checksum()->set_type(CHECKSUM_NULL);
  resp.mutable_readopchecksuminfo()->mutable_checksum()->set_bytesperchecksum(1);
  resp.mutable_readopchecksuminfo()->set_chunkoffset(param.offset());
  os.WriteVarint32(resp.ByteSize());
  resp.SerializeWithCachedSizes(&os);

  int i = 0;
  for (auto it = block.begin(); it != block.end(); it += PACKET_SIZE) {
    if (block.end() - it > PACKET_SIZE) {
      if (!WritePacket(os, raw_os, param.offset() + (it - block.begin()), i, false, PACKET_SIZE, &*it)) {
        fprintf(stderr, "%s:%d write packet failed\n", __func__, __LINE__); fflush(stderr);
        return false;
      }
    } else {
      if (!WritePacket(os, raw_os, param.offset() + (it - block.begin()), i, false, block.end() - it, &*it)) {
        fprintf(stderr, "%s:%d write packet failed\n", __func__, __LINE__); fflush(stderr);
        return false;
      }
      i++;
      if (!WritePacket(os, raw_os, param.offset() + block.size(), i, true, 0, NULL)) {
        fprintf(stderr, "%s:%d write packet failed\n", __func__, __LINE__); fflush(stderr);
        return false;
      }
      break;
    }
    i++;
  }

  return true;
}

static bool ConnectToMirror(const OpWriteBlockProto &param,
                            unique_ptr<FileInputStream> &pfis,
                            unique_ptr<FileOutputStream> &pfos,
                            unique_ptr<CodedInputStream> &pcis,
                            unique_ptr<CodedOutputStream> &pcos) {
  OpWriteBlockProto req(param);
  req.mutable_targets()->erase(req.mutable_targets()->begin());
  if (req.targetstoragetypes_size() > 0)
    req.mutable_targetstoragetypes()->erase(req.mutable_targetstoragetypes()->begin());
  if (req.targetpinnings_size() > 0)
    req.mutable_targetpinnings()->erase(req.mutable_targetpinnings()->begin());
  if (!Connect(param.targets(0).id().ipaddr(), param.targets(0).id().xferport(), pfis, pfos)) {
    fprintf(stderr, "%s:%d connect to %s:%d failed\n", __func__, __LINE__, param.targets(0).id().hostname().c_str(), param.targets(0).id().xferport()); fflush(stderr);
    return false;
  }
  pcos.reset(new CodedOutputStream(&*pfos));
  uint8_t hdr[3] = { 0, 28, 80 };
  pcos->WriteRaw(hdr, sizeof(hdr));
  pcos->WriteVarint32(req.ByteSize());
  req.SerializeWithCachedSizes(&*pcos);
  pcos->Trim();
  pfos->Flush();

  pcis.reset(new CodedInputStream(&*pfis));
  BlockOpResponseProto resp;
  auto limit = pcis->ReadLengthAndPushLimit();
  if (!resp.ParseFromCodedStream(&*pcis)) {
    fprintf(stderr, "%s:%d invalid response\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (!pcis->CheckEntireMessageConsumedAndPopLimit(limit)) {
    fprintf(stderr, "%s:%d invalid response\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (resp.status() != SUCCESS) {
    fprintf(stderr, "%s:%d initialize block write on mirro failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  return true;
}

static bool ConnectToMirror(const OpTransferBlockProto &param,
                            unique_ptr<FileInputStream> &pfis,
                            unique_ptr<FileOutputStream> &pfos,
                            unique_ptr<CodedInputStream> &pcis,
                            unique_ptr<CodedOutputStream> &pcos) {
  OpWriteBlockProto req;
  req.mutable_header()->CopyFrom(param.header());
  req.mutable_targets()->CopyFrom(param.targets());
  req.mutable_targetstoragetypes()->CopyFrom(param.targetstoragetypes());
  req.set_stage(OpWriteBlockProto::PIPELINE_SETUP_CREATE);
  req.set_pipelinesize(param.targets_size());
  req.set_minbytesrcvd(0);
  req.set_maxbytesrcvd(0);
  req.set_latestgenerationstamp(0);
  req.mutable_requestedchecksum()->set_type(CHECKSUM_NULL);
  req.mutable_requestedchecksum()->set_bytesperchecksum(0);

  return ConnectToMirror(req, pfis, pfos, pcis, pcos);
}

bool ReadPacket(CodedInputStream &is, PacketHeaderProto &header, string &data) {
  uint32_t plen;
  uint16_t hlen;
  if (!is.ReadRaw(&plen, sizeof(plen))) {
    fprintf(stderr, "%s:%d invalid packet\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  plen = ntohl(plen);
  if (!is.ReadRaw(&hlen, sizeof(hlen))) {
    fprintf(stderr, "%s:%d invalid packet\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  hlen = ntohs(hlen);
  if (hlen != 25) {
    fprintf(stderr, "%s:%d invalid packet header length %u\n", __func__, __LINE__, hlen); fflush(stderr);
    return false;
  }
  auto limit = is.PushLimit(hlen);
  if (!header.ParseFromCodedStream(&is) || !is.CheckEntireMessageConsumedAndPopLimit(limit)) {
    fprintf(stderr, "%s:%d invalid packet header\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  is.Skip(plen - sizeof(plen) - header.datalen());
  data.resize(header.datalen());
  if (!is.ReadRaw((void *) data.data(), header.datalen())) {
    fprintf(stderr, "%s:%d read packet data failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  return true;
}

bool DataNode::_WriteBlock(CodedInputStream &is, CodedOutputStream &os, FileOutputStream &raw_os) {
  // Read request
  auto limit = is.ReadLengthAndPushLimit();
  OpWriteBlockProto param;
  if (!param.ParseFromCodedStream(&is) || !is.CheckEntireMessageConsumedAndPopLimit(limit)) {
    BlockOpResponseProto resp;
    resp.set_status(ERROR);
    os.WriteVarint32(resp.ByteSize());
    resp.SerializeWithCachedSizes(&os);
    fprintf(stderr, "%s:%d bad request\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  unique_ptr<FileInputStream> pfis;
  unique_ptr<FileOutputStream> pfos;
  unique_ptr<CodedInputStream> pcis;
  unique_ptr<CodedOutputStream> pcos;
  if (param.targets_size() > 0) {
    try {
      // Connect to mirror
      if (!ConnectToMirror(param, pfis, pfos, pcis, pcos)) {
        BlockOpResponseProto resp;
        resp.set_status(ERROR);
        os.WriteVarint32(resp.ByteSize());
        resp.SerializeWithCachedSizes(&os);
        fprintf(stderr, "%s:%d connect to mirror failed\n", __func__, __LINE__); fflush(stderr);
        return false;
      }
    } catch (...) {
      BlockOpResponseProto resp;
      resp.set_status(ERROR);
      os.WriteVarint32(resp.ByteSize());
      resp.SerializeWithCachedSizes(&os);
      fprintf(stderr, "%s:%d connect to mirror failed\n", __func__, __LINE__); fflush(stderr);
      return false;
    }
  }

  // Send response
  BlockOpResponseProto resp;
  resp.set_status(SUCCESS);
  resp.set_firstbadlink("");
  os.WriteVarint32(resp.ByteSize());
  resp.SerializeWithCachedSizes(&os);
  os.Trim();
  raw_os.Flush();

  // Read packets
  for (;;) {
    PacketHeaderProto header;
    string data;
    if (!ReadPacket(is, header, data)) {
      PipelineAckProto resp;
      resp.set_seqno(header.seqno());
      resp.add_reply(ERROR);
      os.WriteVarint32(resp.ByteSize());
      resp.SerializeWithCachedSizes(&os);
      fprintf(stderr, "%s:%d invalid packet\n", __func__, __LINE__); fflush(stderr);
      return false;
    }
    if (!WriteBlock(param.header().baseheader().block().blockid(), header.offsetinblock(), data.size(), data)) {
      PipelineAckProto resp;
      resp.set_seqno(header.seqno());
      resp.add_reply(ERROR);
      os.WriteVarint32(resp.ByteSize());
      resp.SerializeWithCachedSizes(&os);
      fprintf(stderr, "%s:%d write block to extent server failed\n", __func__, __LINE__); fflush(stderr);
      return false;
    }
    PipelineAckProto resp;
    resp.set_seqno(header.seqno());
    resp.add_reply(SUCCESS);
    if (param.targets_size() != 0) {
      do {
        if (!WritePacket(*pcos, *pfos, header, data.data())) {
          for (int i = 0; i < param.targets_size(); i++)
            resp.add_reply(ERROR);
          break;
        }
        PipelineAckProto mirror_resp;
        limit = pcis->ReadLengthAndPushLimit();
        if (!mirror_resp.ParseFromCodedStream(&*pcis)) {
          for (int i = 0; i < param.targets_size(); i++)
            resp.add_reply(ERROR);
          break;
        }
        if (!pcis->CheckEntireMessageConsumedAndPopLimit(limit)) {
          for (int i = 0; i < param.targets_size(); i++)
            resp.add_reply(ERROR);
          break;
        }
        for (auto it = mirror_resp.reply().begin(); it != mirror_resp.reply().end(); it++)
          resp.add_reply((Status) *it);
      } while (0);
    }
    os.WriteVarint32(resp.ByteSize());
    resp.SerializeWithCachedSizes(&os);
    os.Trim();
    raw_os.Flush();
    if (header.lastpacketinblock())
      break;
  }

  return true;
}

bool DataNode::_TransferBlock(google::protobuf::io::CodedInputStream &is, google::protobuf::io::CodedOutputStream &os) {
  // Read request
  auto limit = is.ReadLengthAndPushLimit();
  OpTransferBlockProto param;
  if (!param.ParseFromCodedStream(&is) || !is.CheckEntireMessageConsumedAndPopLimit(limit)) {
    fprintf(stderr, "%s:%d invalid request\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  // Read block
  string block;
  if (!ReadBlock(param.header().baseheader().block().blockid(), 0, BLOCK_SIZE, block)) {
    fprintf(stderr, "%s:%d read block from extent server failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  // Connect to mirror
  unique_ptr<FileInputStream> pfis(nullptr);
  unique_ptr<FileOutputStream> pfos(nullptr);
  unique_ptr<CodedInputStream> pcis(nullptr);
  unique_ptr<CodedOutputStream> pcos(nullptr);
  if (!ConnectToMirror(param, pfis, pfos, pcis, pcos)) {
    fprintf(stderr, "%s:%d connect to mirror failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  // Send block to mirror
  PipelineAckProto ack;
  if (!WritePacket(*pcos, *pfos, 0, 0, false, BLOCK_SIZE, block.data())) {
    fprintf(stderr, "%s:%d send packet to mirror failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  limit = pcis->ReadLengthAndPushLimit();
  if (!ack.ParseFromCodedStream(&*pcis) || !pcis->CheckEntireMessageConsumedAndPopLimit(limit)) {
    fprintf(stderr, "%s:%d invalid pipeline ack\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (ack.reply(0) != SUCCESS) {
    fprintf(stderr, "%s:%d mirror report an error\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (!WritePacket(*pcos, *pfos, BLOCK_SIZE, 1, true, 0, NULL)) {
    fprintf(stderr, "%s:%d send packet to mirror failed\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  limit = pcis->ReadLengthAndPushLimit();
  if (!ack.ParseFromCodedStream(&*pcis) || !pcis->CheckEntireMessageConsumedAndPopLimit(limit)) {
    fprintf(stderr, "%s:%d invalid pipeline ack\n", __func__, __LINE__); fflush(stderr);
    return false;
  }
  if (ack.reply(0) != SUCCESS) {
    fprintf(stderr, "%s:%d mirror report an error\n", __func__, __LINE__); fflush(stderr);
    return false;
  }

  // Send response
  BlockOpResponseProto resp;
  resp.set_status(SUCCESS);
  os.WriteVarint32(resp.ByteSize());
  resp.SerializeWithCachedSizes(&os);
  return true;
}

void *worker(void *_arg) {
  int clientfd = (int) (uintptr_t) _arg;
  FileInputStream fis(clientfd);
  FileOutputStream fos(clientfd);
  fos.SetCloseOnDelete(true);
  CodedInputStream cis(&fis);
  CodedOutputStream cos(&fos);
  bool stop = false;

  while (!stop) {
    int op;
    if (!ReadOp(cis, op))
      break;

    switch (op) {
    case 81: // Read block
      if (!datanode._ReadBlock(cis, cos, fos))
        stop = true;
      break;
    case 80: // Write block
      if (!datanode._WriteBlock(cis, cos, fos))
        stop = true;
      break;
    case 86: // Transfer block
      if (!datanode._TransferBlock(cis, cos)) {
        BlockOpResponseProto resp;
        resp.set_status(ERROR);
        cos.WriteVarint32(resp.ByteSize());
        resp.SerializeWithCachedSizes(&cos);
        stop = true;
      }
      break;
    default:
      fprintf(stderr, "Unknown op %d\n", op);
      fflush(stderr);
      return NULL;
    }

    cos.Trim();
    fos.Flush();
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <extent_server> <namenode>\n", argv[0]);
    fflush(stderr);
    exit(1);
  }

  int acceptfd = socket(AF_INET, SOCK_STREAM, 0);
  if (acceptfd < 0) {
    fprintf(stderr, "Create listen socket failed\n");
    return 1;
  }

  int yes = 1;
  if (setsockopt(acceptfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
    fprintf(stderr, "Enable SO_REUSEADDR failed: %s\n", strerror(errno));
    fflush(stderr);
    return 1;
  }

  struct sockaddr_in bindaddr;
  memset(&bindaddr, 0, sizeof(bindaddr));
  bindaddr.sin_family = AF_INET;
  if (!GetPrimaryIP(&bindaddr.sin_addr))
    return 1;
  bindaddr.sin_port = htons(50010);
  socklen_t addrlen = sizeof(bindaddr);
  if (bind(acceptfd, (struct sockaddr *) &bindaddr, sizeof(bindaddr)) != 0) {
    fprintf(stderr, "Bind %s:50010 failed: %s\n", inet_ntoa(bindaddr.sin_addr), strerror(errno));
    fflush(stderr);
    return 1;
  }
  if (listen(acceptfd, 1) != 0) {
    fprintf(stderr, "Listen on :0 failed: %s\n", strerror(errno));
    fflush(stderr);
    return 1;
  }
  if (getsockname(acceptfd, (struct sockaddr *) &bindaddr, &addrlen) != 0) {
    fprintf(stderr, "Get bind address failed: %s\n", strerror(errno));
    fflush(stderr);
    return 1;
  }

  if (datanode.init(argv[1], argv[2], &bindaddr) != 0)
    return 1;

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
