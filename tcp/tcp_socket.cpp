#include "tcp_socket.h"
#include "tcp_head.h"
#include "log.h"
#include "utility_net.h"
#include <MSWSock.h>
#pragma comment(lib, "Mswsock.lib")

namespace net {

TcpSocket::TcpSocket() {
  ResetMember();
}

TcpSocket::~TcpSocket() {
  Destroy();
}

void TcpSocket::ResetMember() {
  callback_.reset();
  socket_ = INVALID_SOCKET;
  bind_ = false;
  listen_ = false;
  connect_ = false;
  current_head_.clear();
  current_packet_.reset();
  current_packet_offset_ = 0;
  all_packets_.clear();
}

bool TcpSocket::Create(const std::weak_ptr<NetInterface>& callback) {
  if (socket_ != INVALID_SOCKET) {
    LOG(kError, "create tcp socket failed: already created.");
    return false;
  }
  if (callback.expired()) {
    LOG(kError, "create tcp socket failed: invalid callback parameter.");
    return false;
  }
  callback_ = callback;
  socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "create tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  return true;
}

void TcpSocket::Destroy() {
  if (socket_ != INVALID_SOCKET) {
    shutdown(socket_, SD_SEND);
    closesocket(socket_);
    ResetMember();
  }
}

bool TcpSocket::Bind(const std::string& ip, int port) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "bind tcp socket failed: not created.");
    return false;
  }
  if (bind_) {
    LOG(kError, "bind tcp socket failed: already bound.");
    return false;
  }
  auto no_delay = TRUE;
  if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, (const char*)&no_delay, sizeof(no_delay)) != 0) {
    LOG(kError, "set tcp socket nodelay option failed, error code: %d.", WSAGetLastError());
    return false;
  }
  auto reuse_addr = TRUE;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse_addr, sizeof(reuse_addr)) != 0) {
    LOG(kError, "set tcp socket reuse address option failed, error code: %d.", WSAGetLastError());
    return false;
  }
  SOCKADDR_IN bind_addr = {0};
  utility::ToSockAddr(ip, port, bind_addr);
  if (bind(socket_, (SOCKADDR*)&bind_addr, sizeof(bind_addr)) != 0) {
    LOG(kError, "bind tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  bind_ = true;
  return true;
}

bool TcpSocket::Listen(int backlog) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "listen tcp socket failed: not created.");
    return false;
  }
  if (!bind_) {
    LOG(kError, "listen tcp socket failed: not bound.");
    return false;
  }
  if (listen_) {
    LOG(kError, "listen tcp socket failed: already listened.");
    return false;
  }
  if (listen(socket_, backlog) != 0) {
    LOG(kError, "listen tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  listen_ = true;
  return true;
}

bool TcpSocket::Connect(const std::string& ip, int port) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "connect tcp socket failed: not created.");
    return false;
  }
  if (connect_) {
    LOG(kError, "connect tcp socket failed: already connected.");
    return false;
  }
  SOCKADDR_IN connect_addr = {0};
  utility::ToSockAddr(ip, port, connect_addr);
  if (connect(socket_, (SOCKADDR*)&connect_addr, sizeof(connect_addr)) != 0) {
    LOG(kError, "connect tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  connect_ = true;
  return true;
}

bool TcpSocket::AsyncAccept(SOCKET accept_sock, char* buffer, int size, LPOVERLAPPED ovlp) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "async tcp socket accept buffer failed: not created.");
    return false;
  }
  if (!listen_) {
    LOG(kError, "async tcp socket accept buffer failed: not listening.");
    return false;
  }
  int addr_size = sizeof(SOCKADDR_IN) + 16;
  if (accept_sock == INVALID_SOCKET || buffer == nullptr || size < addr_size || ovlp == NULL){
    LOG(kError, "async tcp socket accept buffer failed: invalid parameter.");
    return false;
  }
  DWORD bytes_received = 0;
  if (!AcceptEx(socket_, accept_sock, buffer, 0, addr_size, addr_size, &bytes_received, ovlp)) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "AcceptEx failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool TcpSocket::AsyncSend(const TcpHead* head, const char* buffer, int size, LPOVERLAPPED ovlp) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "async tcp socket send buffer failed: not created.");
    return false;
  }
  if (!connect_) {
    LOG(kError, "async tcp socket send buffer failed: not connected.");
    return false;
  }
  if (buffer == nullptr || size == 0 || ovlp == NULL) {
    LOG(kError, "async tcp socket send buffer failed: invalid parameter.");
    return false;
  }
  WSABUF buff[2] = {0};
  buff[0].buf = (char*)(head);
  buff[0].len = kTcpHeadSize;
  buff[1].buf = const_cast<char*>(buffer);
  buff[1].len = size;
  if (WSASend(socket_, buff, 2, NULL, 0, ovlp, NULL) != 0) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "WSASend failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool TcpSocket::AsyncRecv(char* buffer, int size, LPOVERLAPPED ovlp) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "async tcp socket recv buffer failed: not created.");
    return false;
  }
  if (!connect_) {
    LOG(kError, "async tcp socket recv buffer failed: not connected.");
    return false;
  }
  if (buffer == nullptr || size == 0 || ovlp == NULL) {
    LOG(kError, "async tcp socket recv buffer failed: invalid parameter.");
    return false;
  }
  WSABUF buff = {0};
  buff.buf = buffer;
  buff.len = size;
  DWORD received_flag = 0;
  if (WSARecv(socket_, &buff, 1, NULL, &received_flag, ovlp, NULL) != 0) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "WSARecv failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool TcpSocket::SetAccepted(SOCKET listen_sock) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "set tcp socket accept context failed: not created.");
    return false;
  }
  if (listen_sock == INVALID_SOCKET) {
    LOG(kError, "set tcp socket accept context failed: invalid parameter.");
    return false;
  }
  if (0 != setsockopt(socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&listen_sock, sizeof(listen_sock))) {
    LOG(kError, "set tcp socket accept context failed, error code: %d.", WSAGetLastError());
    return false;
  }
  bind_ = true;
  connect_ = true;
  return true;
}

bool TcpSocket::GetLocalAddr(std::string& ip, int& port) {
  SOCKADDR_IN addr = {0};
  int size = sizeof(addr);
  if (getsockname(socket_, (SOCKADDR*)&addr, &size) != 0) {
    LOG(kError, "getsockname failed, error code: %d.", WSAGetLastError());
    return false;
  }
  utility::FromSockAddr(addr, ip, port);
  return true;
}

bool TcpSocket::GetRemoteAddr(std::string& ip, int& port) {
  SOCKADDR_IN addr = {0};
  int size = sizeof(addr);
  if (getpeername(socket_, (SOCKADDR*)&addr, &size) != 0) {
    LOG(kError, "getsockname failed, error code: %d.", WSAGetLastError());
    return false;
  }
  utility::FromSockAddr(addr, ip, port);
  return true;
}

// two steps for looping parse a recv buffer: first head part then packet part
// if head flag is invalid or packet length too large, parse head part will fail
bool TcpSocket::OnRecv(const char* data, int size) {
  if (data == nullptr || size == 0) {
    return false;
  }
  auto total_parsed = 0;
  while (total_parsed < size) {
    auto current_parsed = 0;
    if (!ParseTcpHead(&data[total_parsed], size - total_parsed, current_parsed)) {
      return false;
    }
    total_parsed += current_parsed;
    if (total_parsed >= size) {
      break;
    }
    current_parsed = ParseTcpPacket(&data[total_parsed], size - total_parsed);
    total_parsed += current_parsed;
  }
  return true;
}

// parse until current_head_'s size reach to the tcp head size
// if the head part is successfully parsed, calculate the packet part size, then reset packet part related member
bool TcpSocket::ParseTcpHead(const char* data, int size, int& parsed_size) {
  if (current_head_.size() < kTcpHeadSize) {
    int left_head_size = kTcpHeadSize - current_head_.size();
    if (left_head_size > size) {
      for (auto i = 0; i < size; ++i) {
        current_head_.push_back(data[i]);
      }
      parsed_size = size;
    } else {
      for (auto i = 0; i < left_head_size; ++i) {
        current_head_.push_back(data[i]);
      }
      parsed_size = left_head_size;
      if (!ResetCurrentPacket()) {
        return false;
      }
    }
  }
  return true;
}

int TcpSocket::ParseTcpPacket(const char* data, int size) {
  auto parsed_size = 0;
  if (current_packet_offset_ == 0) {// packet part begin
    if (current_packet_->size_ > size) {// packet part size bigger than data size
      current_packet_->packet_ = new char[current_packet_->size_];
      current_packet_->deleter_ = std::default_delete<char[]>();
      memcpy(current_packet_->packet_, data, size);
      current_packet_offset_ += size;
      parsed_size = size;
    } else {// data is enough for packet part, generate a packet then push to all_packets_ and reset head part member
      current_packet_->packet_ = const_cast<char*>(data);
      parsed_size = current_packet_->size_;
      all_packets_.push_back(std::move(current_packet_));
      ResetCurrentHead();
    }
  } else {// continue with last packet
    auto left_packet_size = current_packet_->size_ - current_packet_offset_;
    if (left_packet_size > 0 && current_packet_->deleter_ != nullptr) {// assert: must be true
      auto copy_begin = current_packet_->packet_ + current_packet_offset_;
      if (left_packet_size > size) {
        memcpy(copy_begin, data, size);
        current_packet_offset_ += size;
        parsed_size = size;
      } else {
        memcpy(copy_begin, data, left_packet_size);
        all_packets_.push_back(std::move(current_packet_));
        parsed_size = left_packet_size;
        ResetCurrentHead();
      }
    } else {
      LOG(kError, "parse tcp packet assert error.");
    }
  }
  return parsed_size;
}

bool TcpSocket::ResetCurrentPacket() {
  if (!current_head_.empty()) {
    TcpHead head;
    if (!head.Init(&current_head_[0], current_head_.size())) {
      return false;
    }
    current_packet_.reset(new RecvPacket);
    current_packet_->size_ = head.size();
    current_packet_offset_ = 0;
    return true;
  }
  return false;
}

void TcpSocket::ResetCurrentHead() {
  current_head_.clear();
}

} // namespace net