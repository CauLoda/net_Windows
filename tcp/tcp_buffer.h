#ifndef NET_TCP_BUFFER_H_
#define NET_TCP_BUFFER_H_

#include "base_buffer.h"
#include "tcp_head.h"
#include <functional>
#include <memory>

namespace net {

const int kTcpAcceptBuffSize = 64;
const int kTcpBufferSize = 2048;

class TcpSendBuffer : public BaseBuffer {
 public:
  TcpSendBuffer() {
    set_async_type(kAsyncTypeTcpSend);
  }
  ~TcpSendBuffer() {}
  void set_buffer(std::unique_ptr<char[]>&& buffer, int size) {
    buffer_ = std::move(buffer);
    head_.Init(size);
    set_buffer_size(size);
  }
  const TcpHead* head() const { return &head_; }
  const char* buffer() const { return buffer_.get(); }

 private:
  TcpHead head_;
  std::unique_ptr<char[]> buffer_;
};

class TcpRecvBuffer : public BaseBuffer {
 public:
  TcpRecvBuffer() {
    set_async_type(kAsyncTypeTcpRecv);
    set_buffer_size(sizeof(buffer_));
  }
  char* buffer() { return buffer_; }

 private:
  char buffer_[kTcpBufferSize];
};

class TcpSocket;

class TcpAcceptBuffer : public BaseBuffer {
 public:
  TcpAcceptBuffer() {
    set_async_type(kAsyncTypeTcpAccept);
    set_buffer_size(sizeof(buffer_));
  }
  char* buffer() { return buffer_; }
  void set_accept_socket(std::unique_ptr<TcpSocket>&& value) { accept_socket_ = std::move(value); }
  std::unique_ptr<TcpSocket> accept_socket() { return std::move(accept_socket_); }

 private:
  char buffer_[kTcpAcceptBuffSize];
  std::unique_ptr<TcpSocket> accept_socket_;
};

} // namespace net

#endif	// NET_TCP_BUFFER_H_