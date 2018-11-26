#ifndef DATANODE_H_
#define DATANODE_H_

#include <string>
#include <netinet/ip.h>
#include <datanode.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "extent_protocol.h"
#include <memory>

class extent_client;

class DataNode {
private:
  extent_client *ec;
  struct sockaddr_in namenode_addr;
  int namenode_conn;
  bool ConnectToNN();
  bool RegisterOnNamenode();
  static std::string GetHostname();
  static std::string GenerateUUID();
  DatanodeIDProto id;
  bool ReadBlock(blockid_t bid, uint64_t offset, uint64_t len, std::string &buf);
  bool WriteBlock(blockid_t bid, uint64_t offset, uint64_t len, const std::string &buf);
  bool SendHeartbeat();

  /* Feel free to add your member variables/functions here */
public:
  int init(const std::string &extent_dst, const std::string &namenode, const struct sockaddr_in *bindaddr);
  bool _ReadBlock(google::protobuf::io::CodedInputStream &is, google::protobuf::io::CodedOutputStream &os, google::protobuf::io::FileOutputStream &raw_os);
  bool _WriteBlock(google::protobuf::io::CodedInputStream &is, google::protobuf::io::CodedOutputStream &os, google::protobuf::io::FileOutputStream &raw_os);
  bool _TransferBlock(google::protobuf::io::CodedInputStream &is, google::protobuf::io::CodedOutputStream &os);
};

#endif
