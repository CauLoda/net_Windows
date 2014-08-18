#ifndef NET_TCP_HEAD_H_
#define NET_TCP_HEAD_H_

#include <WinSock2.h>

namespace net {

const unsigned long kTcpBlockFlag = 0x51515151;
const unsigned long kMaxTcpSendPacketSize = 16 * 1024 * 1024;

class TcpHead {
 public:
  TcpHead() : flag_(kTcpBlockFlag), size_(0), checksum_(0) {}
  void Init(unsigned long packet_size) {
    size_ = packet_size;
    flag_ = htonl(flag_);
    size_ = htonl(size_);
    checksum_ = htonl(checksum_);
  }
  bool Init(const char* data, int size) {
    if (data == nullptr || size != sizeof(*this)) {
      return false;
    }
    memcpy(this, data, size);
    flag_ = ntohl(flag_);
    size_ = ntohl(size_);
    checksum_ = ntohl(checksum_);
    if (flag_ != kTcpBlockFlag || size_ > kMaxTcpSendPacketSize) {
      return false;
    }
    return true;
  }
  unsigned long size() const { return size_; }

 private:
  unsigned long flag_;
  unsigned long size_;
  unsigned long checksum_;
};
const int kTcpHeadSize = sizeof(TcpHead);

} // namespace net

#endif // NET_TCP_HEAD_H_