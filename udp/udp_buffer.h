#ifndef NET_UDP_BUFFER_H_
#define NET_UDP_BUFFER_H_

#include "base_buffer.h"
#include <memory>
#include <functional>

namespace net {

const int kUdpBufferSize = 8 * 1024;

class UdpSendBuffer : public BaseBuffer {
 public:
  UdpSendBuffer() {
    set_async_type(kAsyncTypeUdpSend);
  }
  ~UdpSendBuffer() {}
  void set_buffer(std::unique_ptr<char[]>&& buffer, int size) {
    buffer_ = std::move(buffer);
    set_buffer_size(size);
  }
  const char* buffer() const { return buffer_.get(); }

 private:
  std::unique_ptr<char[]> buffer_;
};

class UdpRecvBuffer : public BaseBuffer {
 public:
  UdpRecvBuffer() : addr_size_(sizeof(from_addr_)) {
    set_async_type(kAsyncTypeUdpRecv);
    set_buffer_size(sizeof(buffer_));
    memset(&from_addr_, 0, sizeof(from_addr_));
  }
  void ResetBuffer() {
    BaseBuffer::ResetBuffer();
    memset(&from_addr_, 0, sizeof(from_addr_));
    addr_size_ = sizeof(from_addr_);
  }
  char* buffer() { return buffer_; }
  PSOCKADDR_IN from_addr() { return &from_addr_; }
  PINT addr_size() { return &addr_size_; }

 private:
  char buffer_[kUdpBufferSize];
  SOCKADDR_IN from_addr_;
  INT addr_size_;
};

} // namespace net

#endif	// NET_UDP_BUFFER_H_