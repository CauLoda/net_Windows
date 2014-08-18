#ifndef NET_BASE_BUFFER_H_
#define NET_BASE_BUFFER_H_

#include "uncopyable.h"
#include <WinSock2.h>

namespace net {

const int kAsyncTypeTcpAccept = 1;
const int kAsyncTypeTcpSend = 2;
const int kAsyncTypeTcpRecv = 3;
const int kAsyncTypeUdpSend = 4;
const int kAsyncTypeUdpRecv = 5;

class BaseBuffer : public utility::Uncopyable {
 public:
   BaseBuffer() : async_type_(0), buffer_size_(0), handle_(0) {
    memset(&ovlp_, 0, sizeof(ovlp_));
  }
  void ResetBuffer(){
    memset(&ovlp_, 0, sizeof(ovlp_));
    handle_ = 0;
  }
  LPOVERLAPPED ovlp() { return &ovlp_; }
  int async_type() const { return async_type_; }
  int buffer_size() const { return buffer_size_; }
  unsigned long handle() const { return handle_; }
  void set_handle(unsigned long handle) { handle_ = handle; }
  
 protected:
  void set_async_type(int async_type) { async_type_ = async_type; }
  void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }

 private:
  OVERLAPPED ovlp_;
  int async_type_;
  int buffer_size_;
  unsigned long handle_;
};

} // namespace net

#endif	// NET_BASE_BUFFER_H_