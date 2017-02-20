#ifndef TCP_BUFFER_H_
#define TCP_BUFFER_H_

#define WIN32_LEAN_AND_MEAN
#include "socket.h"
#include <memory>

namespace tcp {

enum class AsyncType {
  kInvalid,
  kConnect,
  kAccept,
  kSend,
  kRecv,
  kDestroy,
};

const int kAcceptBuffSize = 64;
const int kSendBufferSize = 2048;
const int kRecvBufferSize = 2048;

class BaseBuffer : public utility::Uncopyable {
 public:
  BaseBuffer() {
    Reset();
  }
  void Reset() {
    memset(&ovlp_, 0, sizeof(ovlp_));
  }
  LPOVERLAPPED ovlp() { return &ovlp_; }
  AsyncType async_type() { return async_type_; }
  int buffer_size() { return buffer_size_; }
  unsigned long handle() { return handle_; }
  void set_handle(unsigned long handle) { handle_ = handle; }

 protected:
  void set_async_type(AsyncType async_type) { async_type_ = async_type; }
  void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }

 private:
  OVERLAPPED ovlp_ = { 0 };
  AsyncType async_type_ = AsyncType::kInvalid;
  int buffer_size_ = 0;
  unsigned long handle_ = 0;
};

class SendBuffer : public BaseBuffer {
 public:
  SendBuffer() {
    set_async_type(AsyncType::kSend);
  }
  bool set_buffer(const char* buffer, int size) {
    if (size <= 0 || size > sizeof(buffer_)) {
      return false;
    }
    memcpy(buffer_, buffer, size);
    set_buffer_size(size);
    return true;
  }
  const char* buffer() { return buffer_; }

 private:
  char buffer_[kSendBufferSize];
};

class RecvBuffer : public BaseBuffer {
 public:
  RecvBuffer() {
    set_async_type(AsyncType::kRecv);
    set_buffer_size(sizeof(buffer_));
  }
  char* buffer() { return buffer_; }

 private:
  char buffer_[kRecvBufferSize];
};

class AcceptBuffer : public BaseBuffer {
 public:
  AcceptBuffer() {
    set_async_type(AsyncType::kAccept);
    set_buffer_size(sizeof(buffer_));
  }
  void Reset() {
    BaseBuffer::Reset();
    accept_socket_.reset();
  }
  char* buffer() { return buffer_; }
  void set_accept_socket(std::unique_ptr<Socket> socket) { accept_socket_ = std::move(socket); }
  std::unique_ptr<Socket> accept_socket() { return std::move(accept_socket_); }

 private:
  char buffer_[kAcceptBuffSize];
  std::unique_ptr<Socket> accept_socket_;
};

} // namespace tcp

#endif	// TCP_BUFFER_H_