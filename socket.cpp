#include "socket.h"
#include "../utility/log.h"
#include "../utility/utility_net.h"
#include <Mstcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace tcp {

Socket::~Socket() {
  if (socket_ != INVALID_SOCKET) {
    shutdown(socket_, SD_SEND);
    closesocket(socket_);
  }
}

bool Socket::Create() {
  socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "create tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  return true;
}

bool Socket::Bind(const std::string& ip, int port) {
  BOOL no_delay = TRUE;
  if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, (const char*)&no_delay, sizeof(no_delay)) != 0) {
    LOG(kError, "set tcp socket nodelay option failed, error code: %d.", WSAGetLastError());
    return false;
  }
  SOCKADDR_IN bind_addr = { 0 };
  utility::ToSockAddr(ip, port, bind_addr);
  if (bind(socket_, (SOCKADDR*)&bind_addr, sizeof(bind_addr)) != 0) {
    LOG(kError, "bind tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  return true;
}

bool Socket::Listen(int backlog) {
  if (listen(socket_, backlog) != 0) {
    LOG(kError, "listen tcp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  return true;
}

bool Socket::Connect(const std::string& ip, int port, int timeout) {
  if (!SetNonblockingMode(true)) {
    return false;
  }
  SOCKADDR_IN connect_addr = { 0 };
  utility::ToSockAddr(ip, port, connect_addr);
  if (connect(socket_, (SOCKADDR*)&connect_addr, sizeof(connect_addr)) == SOCKET_ERROR) {
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
      LOG(kError, "connect tcp socket failed, error code: %d.", WSAGetLastError());
      SetNonblockingMode(false);
      return false;
    }
    fd_set connect_set;
    FD_ZERO(&connect_set);
    FD_SET(socket_, &connect_set);
    timeval timeout_time = { timeout / 1000, timeout % 1000 };
    auto ret = select(0, NULL, &connect_set, NULL, &timeout_time);
    if (ret == SOCKET_ERROR) {
      LOG(kError, "select connect state failed, error code: %d.", WSAGetLastError());
      SetNonblockingMode(false);
      return false;
    } else if (ret == 0) {
      LOG(kError, "connect tcp socket failed: time out.");
      SetNonblockingMode(false);
      return false;
    }
  }
  SetNonblockingMode(false);
  return true;
}

bool Socket::SetNonblockingMode(bool nonblocking) {
  u_long io_arg = nonblocking ? 1 : 0;
  if (ioctlsocket(socket_, FIONBIO, &io_arg) == SOCKET_ERROR) {
    LOG(kError, "ioctlsocket nonblocking mode:%u failed, error code: %d.", io_arg, WSAGetLastError());
    return false;
  }
  return true;
}

bool Socket::AsyncAccept(SOCKET accept_sock, char* buffer, int size, LPOVERLAPPED ovlp) {
  auto addr_size = sizeof(SOCKADDR_IN) + 16;
  DWORD bytes_received = 0;
  if (!AcceptEx(socket_, accept_sock, buffer, 0, addr_size, addr_size, &bytes_received, ovlp)) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "AcceptEx failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool Socket::AsyncSend(const char* buffer, int size, LPOVERLAPPED ovlp) {
  WSABUF buff = { 0 };
  buff.buf = const_cast<char*>(buffer);
  buff.len = size;
  if (WSASend(socket_, &buff, 1, NULL, 0, ovlp, NULL) != 0) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "WSASend failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool Socket::AsyncRecv(char* buffer, int size, LPOVERLAPPED ovlp) {
  WSABUF buff = { 0 };
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

bool Socket::SetAccepted(SOCKET listen_sock) {
  if (0 != setsockopt(socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&listen_sock, sizeof(listen_sock))) {
    LOG(kError, "set tcp socket accept context failed, error code: %d.", WSAGetLastError());
    return false;
  }
  return true;
}

bool Socket::GetLocalAddr(std::string& ip, int& port) {
  SOCKADDR_IN addr = { 0 };
  int size = sizeof(addr);
  if (getsockname(socket_, (SOCKADDR*)&addr, &size) != 0) {
    LOG(kError, "getsockname failed, error code: %d.", WSAGetLastError());
    return false;
  }
  utility::FromSockAddr(addr, ip, port);
  return true;
}

bool Socket::GetRemoteAddr(std::string& ip, int& port) {
  SOCKADDR_IN addr = { 0 };
  int size = sizeof(addr);
  if (getpeername(socket_, (SOCKADDR*)&addr, &size) != 0) {
    LOG(kError, "getsockname failed, error code: %d.", WSAGetLastError());
    return false;
  }
  utility::FromSockAddr(addr, ip, port);
  return true;
}

} // namespace tcp