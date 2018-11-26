#ifndef HRPC_H_
#define HRPC_H_

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <fcntl.h>

template<typename T>
size_t GetDelimitedLength(const T &message) {
  size_t length = message.ByteSize();
  return length + google::protobuf::io::CodedOutputStream::VarintSize64(length);
}

template<typename T1>
bool HrpcWrite(int fd, const T1 &a1) {
  google::protobuf::io::FileOutputStream fos(fd);
  google::protobuf::io::CodedOutputStream cos(&fos);
  uint32_t len = GetDelimitedLength(a1);
  len = htonl(len);
  cos.WriteRaw(&len, sizeof(len));
  cos.WriteVarint32(a1.ByteSize());
  a1.SerializeWithCachedSizes(&cos);
  cos.Trim();
  return !cos.HadError();
}

template<typename T1, typename T2>
bool HrpcWrite(int fd, const T1 &a1, const T2 &a2) {
  google::protobuf::io::FileOutputStream fos(fd);
  google::protobuf::io::CodedOutputStream cos(&fos);
  uint32_t len = GetDelimitedLength(a1);
  len += GetDelimitedLength(a2);
  len = htonl(len);
  cos.WriteRaw(&len, sizeof(len));
  cos.WriteVarint32(a1.ByteSize());
  a1.SerializeWithCachedSizes(&cos);
  cos.WriteVarint32(a2.ByteSize());
  a2.SerializeWithCachedSizes(&cos);
  cos.Trim();
  return !cos.HadError();
}

template<typename T1, typename T2, typename T3>
bool HrpcWrite(int fd, const T1 &a1, const T2 &a2, const T3 &a3) {
  google::protobuf::io::FileOutputStream fos(fd);
  google::protobuf::io::CodedOutputStream cos(&fos);
  uint32_t len = GetDelimitedLength(a1);
  len += GetDelimitedLength(a2);
  len += GetDelimitedLength(a3);
  len = htonl(len);
  cos.WriteRaw(&len, sizeof(len));
  cos.WriteVarint32(a1.ByteSize());
  a1.SerializeWithCachedSizes(&cos);
  cos.WriteVarint32(a2.ByteSize());
  a2.SerializeWithCachedSizes(&cos);
  cos.WriteVarint32(a3.ByteSize());
  a3.SerializeWithCachedSizes(&cos);
  cos.Trim();
  return !cos.HadError();
}

static int ReadVariant(const uint8_t **p, const uint8_t *pend, uint64_t *v) {
  int shift = 0;
  uint64_t res = 0;
  while (*p != pend && **p >= 0x80) {
    res |= (((**p) & 0x7f) << shift);
    shift += 7;
    (*p)++;
  }
  if (*p == pend)
    return -1;
  res |= (((**p) & 0x7f) << shift);
  (*p)++;
  *v = res;
  return 0;
}

template<typename T>
int ReadDelimited(const uint8_t **p, const uint8_t *pend, T &t) {
  uint64_t len;
  if (ReadVariant(p, pend, &len) != 0)
    return -1;
  if (*p + len > pend)
    return -1;
  if (!t.ParseFromArray(*p, len))
    return -1;
  (*p) += len;
  return 0;
}

template<typename T>
int ReadDelimited(std::string &buf, T &t) {
  uint64_t len;
  const uint8_t *p = (const uint8_t *) buf.data(), *pend = (const uint8_t *) buf.data() + buf.size();
  if (ReadVariant(&p, pend, &len) != 0)
    return -1;
  if (p + len > pend)
    return -1;
  if (!t.ParseFromArray(p, len))
    return -1;
  buf = buf.substr(p - (const uint8_t *) buf.data() + len);
  return 0;
}

bool HrpcRead(int fd, std::string &a1) {
  uint32_t len;
  ssize_t ret;
  if (read(fd, &len, sizeof(len)) != sizeof(len))
    return false;
  len = ntohl(len);

  a1.resize(len);
  if ((ret = read(fd, (void *) a1.data(), len)) != len)
    return false;

  return true;
}

template<typename T1>
bool HrpcRead(int fd, T1 &a1, std::string &a2) {
  if (!HrpcRead(fd, a2))
    return false;

  if (ReadDelimited(a2, a1) != 0)
    return false;
  return true;
}

template<typename T1, typename T2>
bool HrpcRead(int fd, T1 &a1, T2 &a2, std::string &a3) {
  if (!HrpcRead(fd, a1, a3))
    return false;

  if (ReadDelimited(a3, a2) != 0)
    return false;
  return true;
}

static __attribute__((used))
bool Connect(const std::string &ip, short port,
             std::unique_ptr<google::protobuf::io::FileInputStream> &pfis,
             std::unique_ptr<google::protobuf::io::FileOutputStream> &pfos)  {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1)
    return false;
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  if (inet_aton(ip.c_str(), &addr.sin_addr) == 0) {
    close(fd);
    return false;
  }
  addr.sin_port = htons(port);
  if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
    close(fd);
    return false;
  }
  pfis.reset(new google::protobuf::io::FileInputStream(fd));
  pfos.reset(new google::protobuf::io::FileOutputStream(fd));
  pfos->SetCloseOnDelete(true);
  return true;
}

#endif
