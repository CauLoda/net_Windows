#include "net_res_mgr.h"
#include "utility/log.h"
#include "utility/utility.h"
#include "utility/utility_net.h"
#pragma comment(lib, "Ws2_32.lib")

namespace net {

bool NetResMgr::StartupNet(NetInterface* callback) {
  InitLog("net_module_");
  callback_ = callback;
  if (!utility::StartupNet()) {
    return false;
  }
  auto iocp_callback = std::bind(&NetResMgr::IocpCallbackFunc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  if (!iocp_.Init(iocp_callback)) {
    return false;
  }
  return true;
}

bool NetResMgr::CleanupNet() {
  iocp_.Uninit();
  tcp_sockets_lock_.lock();
  tcp_sockets_.clear();
  tcp_sockets_lock_.unlock();
  tcp_indexer_.DestroyAllIndex();
  return true;
}

bool NetResMgr::TcpCreate(const std::string& ip, int port, TcpHandle& new_handle) {
  auto new_socket = std::make_shared<tcp::Socket>();
  if (!new_socket) {
    LOG(kError, "TcpCreate failed: can not new tcp socket.");
    return false;
  }
  if (!new_socket->Create()) {
    return false;
  }
  if (!new_socket->Bind(ip, port)) {
    return false;
  }
  if (!iocp_.BindToIocp((HANDLE)new_socket->socket())) {
    return false;
  }
  if (!AddTcpSocket(new_socket, new_handle)) {
    return false;
  }
  return true;
}

bool NetResMgr::TcpDestroy(TcpHandle handle) {
  RemoveTcpSocket(handle);
  return true;
}

bool NetResMgr::TcpListen(TcpHandle handle) {
  auto socket = GetTcpSocket(handle);
  if (!socket) {
    LOG(kError, "TcpListen failed: can not find handle %u.", handle);
    return false;
  }
  auto backlog = utility::GetProcessorNum() * 2;
  if (!socket->Listen(backlog)) {
    return false;
  }
  for (auto i = 0; i < backlog; ++i) {
    if (!AsyncTcpAccept(handle, socket, nullptr)) {
      return false;
    }
  }
  return true;
}

bool NetResMgr::TcpConnect(TcpHandle handle, const std::string& ip, int port, int timeout) {
  auto socket = GetTcpSocket(handle);
  if (!socket) {
    LOG(kError, "TcpConnect failed: can not find handle %u.", handle);
    return false;
  }
  if (!socket->Connect(ip, port, timeout)) {
    return false;
  }
  if (!AsyncTcpRecv(handle, socket, nullptr)) {
    return false;
  }
  return true;
}

bool NetResMgr::TcpSend(TcpHandle handle, const char* packet, int size) {
  if (!packet || size <= 0 || size > MAX_TCP_PACKET_SIZE) {
    LOG(kError, "TcpSend failed: invalid parameter of handle %u.", handle);
    return false;
  }
  auto socket = GetTcpSocket(handle);
  if (!socket) {
    return false;
  }
  auto send_buffer = std::make_unique<tcp::SendBuffer>();
  if (!send_buffer) {
    LOG(kError, "TcpSend failed: can not new send buffer.", handle);
    return false;
  }
  send_buffer->set_buffer(packet, size);
  send_buffer->set_handle(handle);
  if (!socket->AsyncSend(send_buffer->buffer(), send_buffer->buffer_size(), send_buffer->ovlp())) {
    return false;
  }
  send_buffer.release();
  return true;
}

bool NetResMgr::TcpGetLocalAddr(TcpHandle handle, char ip[16], int& port) {
  auto socket = GetTcpSocket(handle);
  if (!socket) {
    LOG(kError, "TcpGetLocalAddr failed: can not find handle %u.", handle);
    return false;
  }
  std::string ip_string;
  if (!socket->GetLocalAddr(ip_string, port)) {
    return false;
  }
  strcpy_s(ip, 16, ip_string.c_str());
  return true;
}

bool NetResMgr::TcpGetRemoteAddr(TcpHandle handle, char ip[16], int& port) {
  auto socket = GetTcpSocket(handle);
  if (!socket) {
    LOG(kError, "TcpGetRemoteAddr failed: can not find handle %u.", handle);
    return false;
  }
  std::string ip_string;
  if (!socket->GetRemoteAddr(ip_string, port)) {
    return false;
  }
  strcpy_s(ip, 16, ip_string.c_str());
  return true;
}

bool NetResMgr::AddTcpSocket(const TcpSocketPtr& new_socket, TcpHandle& new_handle) {
  auto new_index = tcp_indexer_.CreateIndex();
  if (new_index == utility::kInvalidIndex) {
    LOG(kError, "AddTcpSocket failed: no useful index.");
    return false;
  }
  new_handle = new_index;
  std::lock_guard<std::mutex> lock(tcp_sockets_lock_);
  tcp_sockets_.insert(std::make_pair(new_handle, new_socket));
  return true;
}

void NetResMgr::RemoveTcpSocket(TcpHandle handle) {
  tcp_sockets_lock_.lock();
  auto socket = tcp_sockets_.find(handle);
  if (socket != tcp_sockets_.end()) {
    tcp_sockets_.erase(socket);
    tcp_sockets_lock_.unlock();
    tcp_indexer_.DestroyIndex(handle);
  } else {
    tcp_sockets_lock_.unlock();
  }
}

TcpSocketPtr NetResMgr::GetTcpSocket(TcpHandle handle) {
  std::lock_guard<std::mutex> lock(tcp_sockets_lock_);
  auto socket = tcp_sockets_.find(handle);
  if (socket == tcp_sockets_.end()) {
    return nullptr;
  }
  return socket->second;
}

bool NetResMgr::AsyncTcpAccept(TcpHandle handle, const TcpSocketPtr& socket, std::unique_ptr<tcp::AcceptBuffer> buffer) {
  auto accept_socket = std::make_unique<tcp::Socket>();
  if (!accept_socket) {
    LOG(kError, "AsyncTcpAccept failed: can not new tcp socket.");
    return false;
  }
  if (!accept_socket->Create()) {
    return false;
  }
  if (!buffer) {
    buffer = std::make_unique<tcp::AcceptBuffer>();
    if (!buffer) {
      LOG(kError, "AsyncTcpAccept failed: can not new accept buffer.");
      return false;
    }
  }
  auto async_sock = accept_socket->socket();
  buffer->Reset();
  buffer->set_handle(handle);
  buffer->set_accept_socket(std::move(accept_socket));
  if (!socket->AsyncAccept(async_sock, buffer->buffer(), buffer->buffer_size(), buffer->ovlp())) {
    return false;
  }
  buffer.release();
  return true;
}

bool NetResMgr::AsyncTcpRecv(TcpHandle handle, const TcpSocketPtr& socket, std::unique_ptr<tcp::RecvBuffer> buffer) {
  if (!buffer) {
    buffer = std::make_unique<tcp::RecvBuffer>();
    if (!buffer) {
      LOG(kError, "AsyncTcpRecv failed: can not new recv buffer.");
      return false;
    }
  }
  buffer->Reset();
  buffer->set_handle(handle);
  if (!socket->AsyncRecv(buffer->buffer(), buffer->buffer_size(), buffer->ovlp())) {
    return false;
  }
  buffer.release();
  return true;
}

void NetResMgr::IocpCallbackFunc(DWORD error_code, DWORD size, LPOVERLAPPED ovlp) {
  if (error_code != ERROR_SUCCESS) {
    if (error_code != ERROR_NETNAME_DELETED &&
        error_code != ERROR_CONNECTION_ABORTED &&
        error_code != ERROR_OPERATION_ABORTED &&
        error_code != ERROR_SEM_TIMEOUT) {
      LOG(kError, "IocpCallbackFunc invalid error code: %d.", error_code);
    }
  }
  auto buffer = (tcp::BaseBuffer*)ovlp;
  switch (buffer->async_type()) {
  case tcp::AsyncType::kAccept:
    OnTcpAccept(std::unique_ptr<tcp::AcceptBuffer>((tcp::AcceptBuffer*)buffer));
    break;
  case tcp::AsyncType::kSend:
    OnTcpSend(std::unique_ptr<tcp::SendBuffer>((tcp::SendBuffer*)buffer));
    break;
  case tcp::AsyncType::kRecv:
    OnTcpRecv(std::unique_ptr<tcp::RecvBuffer>((tcp::RecvBuffer*)buffer), size);
    break;
  default:
    break;
  }
}

void NetResMgr::OnTcpAccept(std::unique_ptr<tcp::AcceptBuffer> buffer) {
  auto listen_handle = buffer->handle();
  auto listen_socket = GetTcpSocket(listen_handle);
  if (!listen_socket) {
    return;
  }
  TcpSocketPtr accept_socket(buffer->accept_socket());
  if (!AsyncTcpAccept(listen_handle, listen_socket, std::move(buffer))) {
    callback_->OnTcpError(listen_handle, 1);
  }
  if (accept_socket->SetAccepted(listen_socket->socket()) && iocp_.BindToIocp((HANDLE)accept_socket->socket())) {
    auto accept_handle = INVALID_TCP_HANDLE;
    if (AddTcpSocket(accept_socket, accept_handle)) {
      callback_->OnTcpAccepted(listen_handle, accept_handle);
      if (!AsyncTcpRecv(accept_handle, accept_socket, nullptr)) {
        RemoveTcpSocket(accept_handle);
        callback_->OnTcpError(accept_handle, 2);
      }
    }
  }
}

void NetResMgr::OnTcpSend(std::unique_ptr<tcp::SendBuffer> buffer) {
}

void NetResMgr::OnTcpRecv(std::unique_ptr<tcp::RecvBuffer> buffer, int size) {
  auto recv_handle = buffer->handle();
  auto recv_socket = GetTcpSocket(recv_handle);
  if (!recv_socket) {
    return;
  }
  if (size == 0) {
    RemoveTcpSocket(recv_handle);
    callback_->OnTcpDisconnected(recv_handle);
  } else {
    callback_->OnTcpReceived(recv_handle, buffer->buffer(), size);
    if (!AsyncTcpRecv(recv_handle, recv_socket, std::move(buffer))) {
      RemoveTcpSocket(recv_handle);
      callback_->OnTcpError(recv_handle, 2);
    }
  }
}

} // namespace net