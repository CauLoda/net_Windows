#include "udp_socket.h"
#include "log.h"
#include "utility_net.h"
#include <MSWSock.h>
#pragma comment(lib, "Mswsock.lib")

namespace net {

UdpSocket::UdpSocket() {
  ResetMember();
}

UdpSocket::~UdpSocket() {
  Destroy();
}

void UdpSocket::ResetMember() {
  callback_.reset();
  socket_ = INVALID_SOCKET;
  bind_ = false;
}

bool UdpSocket::Create(const std::weak_ptr<NetInterface>& callback) {
  if (socket_ != INVALID_SOCKET) {
    LOG(kError, "create udp socket failed: already created.");
    return false;
  }
  if (callback.expired()) {
    LOG(kError, "create udp socket failed: invalid callback parameter.");
    return false;
  }
  socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "create udp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  callback_ = callback;
  auto broadcast_opt = TRUE;
  if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast_opt, sizeof(broadcast_opt)) != 0) {
    LOG(kError, "set udp socket broadcast option failed, error code: %d.", WSAGetLastError());
    Destroy();
    return false;
  }
  return true;
}

void UdpSocket::Destroy() {
  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
    ResetMember();
  }
}

bool UdpSocket::Bind(const std::string& ip, int port) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "bind udp socket failed: not created.");
    return false;
  }
  if (bind_) {
    LOG(kError, "bind udp socket failed: already bound.");
    return false;
  }
  SOCKADDR_IN bind_addr = {0};
  utility::ToSockAddr(ip, port, bind_addr);
  if (bind(socket_, (SOCKADDR*)&bind_addr, sizeof(bind_addr)) != 0) {
    LOG(kError, "bind udp socket failed, error code: %d.", WSAGetLastError());
    return false;
  }
  auto reset_ctl = FALSE;
  DWORD return_bytes = 0;
  if (WSAIoctl(socket_, SIO_UDP_CONNRESET, &reset_ctl, sizeof(reset_ctl), NULL, 0, &return_bytes, NULL, NULL) != 0) {
    LOG(kError, "set udp socket reset control failed, error code: %d.", WSAGetLastError());
    return false;
  }
  bind_ = true;
  return true;
}

bool UdpSocket::AsyncSendTo(const char* buffer, int size, const std::string& ip, int port, LPOVERLAPPED ovlp) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "async udp socket send buffer failed: not created.");
    return false;
  }
  if (buffer == nullptr || size == 0 || ovlp == NULL) {
    LOG(kError, "async udp socket send buffer failed: invalid parameter.");
    return false;
  }
  WSABUF buff = {0};
  buff.buf = const_cast<char*>(buffer);
  buff.len = size;
  SOCKADDR_IN send_to_addr = {0};
  utility::ToSockAddr(ip, port, send_to_addr);
  if (WSASendTo(socket_, &buff, 1, NULL, 0, (PSOCKADDR)&send_to_addr, sizeof(send_to_addr), ovlp, NULL) != 0) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "WSASendTo failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool UdpSocket::AsyncRecvFrom(char* buffer, int size, PSOCKADDR_IN addr, PINT addr_size, LPOVERLAPPED ovlp) {
  if (socket_ == INVALID_SOCKET) {
    LOG(kError, "async udp socket recv buffer failed: not created.");
    return false;
  }
  if (buffer == nullptr || size == 0 || addr == nullptr || addr_size == nullptr || ovlp == NULL) {
    LOG(kError, "async ud socket recv buffer failed: invalid parameter.");
    return false;
  }
  WSABUF buff = {0};
  buff.buf = buffer;
  buff.len = size;
  DWORD received_flag = 0;
  if (WSARecvFrom(socket_, &buff, 1, NULL, &received_flag, (PSOCKADDR)addr, addr_size, ovlp, NULL) != 0) {
    if (WSAGetLastError() != ERROR_IO_PENDING) {
      LOG(kError, "WSARecvFrom failed, error code: %d.", WSAGetLastError());
      return false;
    }
  }
  return true;
}

} // namespace net