#include "net_res_mgr.h"
#include "log.h"
#include "utility.h"
#include "utility_net.h"
#pragma comment(lib, "Ws2_32.lib")

namespace net {

NetResMgr::NetResMgr() {
  net_started_ = false;
}

NetResMgr::~NetResMgr() {
  CleanupNet();
}

bool NetResMgr::StartupNet() {
  if (net_started_) {
    return true;
  }
  net_started_ = true;
  auto iocp_callback = std::bind(&NetResMgr::TransferAsyncType, this, std::placeholders::_1, std::placeholders::_2);
  if (!iocp_.Init(std::move(iocp_callback))) {
    CleanupNet();
    return false;
  }
  return true;
}

bool NetResMgr::CleanupNet() {
  if (!net_started_) {
    return true;
  }
  tcp_sockets_lock_.lock();
  tcp_sockets_.clear();
  tcp_sockets_lock_.unlock();
  tcp_indexer_.Clear();
  udp_sockets_lock_.lock();
  udp_sockets_.clear();
  udp_sockets_lock_.unlock();
  udp_indexer_.Clear();
  iocp_.Uninit();
  net_started_ = false;
  return true;
}

bool NetResMgr::TcpCreate(const std::weak_ptr<NetInterface>& callback, const std::string& ip, int port, TcpHandle& new_handle) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  if (callback.expired()) {
    LOG(kError, "create tcp handle failed: invalid callback parameter.");
    return false;
  }
  auto new_socket = std::make_shared<TcpSocket>();
  if (!new_socket)
  {
    LOG(kError, "create tcp handle failed: not enough memory.");
    return false;
  }
  if (!new_socket->Create(callback)) {
    return false;
  }
  if (!new_socket->Bind(ip, port)) {
    return false;
  }
  if (!iocp_.BindToIOCP(new_socket->socket())) {
    return false;
  }
  if (!AddTcpSocket(new_socket, new_handle)) {
    return false;
  }
  return true;
}

bool NetResMgr::TcpDestroy(TcpHandle handle) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  RemoveTcpSocket(handle);
  return true;
}

bool NetResMgr::TcpListen(TcpHandle handle) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  auto socket = GetTcpSocket(handle);
  if (socket == nullptr) {
    return false;
  }
  auto backlog = utility::GetProcessorNum() * 2;
  if (!socket->Listen(backlog)) {
    return false;
  }
  for (auto i = 0; i < backlog; ++i) {
    auto accept_buffer = GetTcpAcceptBuffer();
    if (accept_buffer == nullptr) {
      return false;
    }
    if (!AsyncTcpAccept(handle, socket, accept_buffer)) {
      return false;
    }
  }
  return true;
}

bool NetResMgr::TcpConnect(TcpHandle handle, const std::string& ip, int port) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  auto socket = GetTcpSocket(handle);
  if (socket == nullptr) {
    return false;
  }
  if (!socket->Connect(ip, port)) {
    return false;
  }
  auto recv_buffer = GetTcpRecvBuffer();
  if (recv_buffer == nullptr) {
    return false;
  }
  if (!AsyncTcpRecv(handle, socket, recv_buffer)) {
    return false;
  }
  return true;
}

bool NetResMgr::TcpSend(TcpHandle handle, std::unique_ptr<char[]>&& packet, int size) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  if (packet == nullptr || size <= 0 || size > kMaxTcpPacketSize) {
    LOG(kError, "send tcp handle: %u packet failed: invalid parameter.", handle);
    return false;
  }
  auto socket = GetTcpSocket(handle);
  if (socket == nullptr) {
    return false;
  }
  auto send_buffer = GetTcpSendBuffer();
  if (send_buffer == nullptr) {
    return false;
  }
  send_buffer->set_buffer(std::move(packet), size);
  send_buffer->set_handle(handle);
  if (!socket->AsyncSend(send_buffer->head(), send_buffer->buffer(), send_buffer->buffer_size(), send_buffer->ovlp())) {
    ReturnTcpSendBuffer(send_buffer);
    return false;
  }
  return true;
}

bool NetResMgr::TcpGetLocalAddr(TcpHandle handle, char ip[16], int& port) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  if (ip == nullptr) {
    LOG(kError, "get tcp handle : %u local address failed: invalid ip parameter.", handle);
    return false;
  }
  auto socket = GetTcpSocket(handle);
  if (socket == nullptr) {
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
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  if (ip == nullptr) {
    LOG(kError, "get tcp handle : %u remote address failed: invalid ip parameter.", handle);
    return false;
  }
  auto socket = GetTcpSocket(handle);
  if (socket == nullptr) {
    return false;
  }
  std::string ip_string;
  if (!socket->GetRemoteAddr(ip_string, port)) {
    return false;
  }
  strcpy_s(ip, 16, ip_string.c_str());
  return true;
}

bool NetResMgr::UdpCreate(const std::weak_ptr<NetInterface>& callback, const std::string& ip, int port, UdpHandle& new_handle) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  if (callback.expired()) {
    LOG(kError, "create udp handle failed: invalid callback parameter.");
    return false;
  }
  auto new_socket = std::make_shared<UdpSocket>();
  if (!new_socket)
  {
    LOG(kError, "create udp handle failed: not enough memory.");
    return false;
  }
  if (!new_socket->Create(callback)) {
    return false;
  }
  if (!new_socket->Bind(ip, port)) {
    return false;
  }
  if (!iocp_.BindToIOCP(new_socket->socket())) {
    return false;
  }
  if (!AddUdpSocket(new_socket, new_handle)) {
    return false;
  }
  auto recv_count = utility::GetProcessorNum();
  for (auto i = 0; i < recv_count; ++i) {
    auto recv_buffer = GetUdpRecvBuffer();
    if (recv_buffer == nullptr) {
      return false;
    }
    if (!AsyncUdpRecv(new_handle, new_socket, recv_buffer)) {
      return false;
    }
  }
  return true;
}

bool NetResMgr::UdpDestroy(UdpHandle handle) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  RemoveUdpSocket(handle);
  return true;
}

bool NetResMgr::UdpSendTo(UdpHandle handle, std::unique_ptr<char[]>&& packet, int size, const std::string& ip, int port) {
  if (!net_started_) {
    LOG(kError, "net not started.");
    return false;
  }
  if (packet == nullptr || size <= 0 || size > kMaxUdpPacketSize || port <= 0) {
    LOG(kError, "send udp handle: %u packet failed: invalid parameter.", handle);
    return false;
  }
  auto socket = GetUdpSocket(handle);
  if (socket == nullptr) {
    return false;
  }
  auto send_buffer = GetUdpSendBuffer();
  if (send_buffer == nullptr) {
    return false;
  }
  send_buffer->set_buffer(std::move(packet), size);
  send_buffer->set_handle(handle);
  if (!socket->AsyncSendTo(send_buffer->buffer(), send_buffer->buffer_size(), ip, port, send_buffer->ovlp())) {
    ReturnUdpSendBuffer(send_buffer);
    return false;
  }
  return true;
}

bool NetResMgr::AddTcpSocket(const std::shared_ptr<TcpSocket>& new_socket, TcpHandle& new_handle) {
  auto new_index = tcp_indexer_.CreateIndex();
  if (new_index == utility::kInvalidIndex) {
    LOG(kError, "fail to new tcp handle: no useful index.");
    return false;
  }
  new_handle = new_index;
  std::lock_guard<std::mutex> lock(tcp_sockets_lock_);
  tcp_sockets_.insert(std::make_pair(new_handle, new_socket));
  return true;
}

bool NetResMgr::AddUdpSocket(const std::shared_ptr<UdpSocket>& new_socket, TcpHandle& new_handle) {
  auto new_index = udp_indexer_.CreateIndex();
  if (new_index == utility::kInvalidIndex) {
    LOG(kError, "fail to new udp handle: no useful index.");
    return false;
  }
  new_handle = new_index;
  std::lock_guard<std::mutex> lock(udp_sockets_lock_);
  udp_sockets_.insert(std::make_pair(new_handle, new_socket));
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

void NetResMgr::RemoveUdpSocket(UdpHandle handle) {
  udp_sockets_lock_.lock();
  auto socket = udp_sockets_.find(handle);
  if (socket != udp_sockets_.end()) {
    udp_sockets_.erase(socket);
    udp_sockets_lock_.unlock();
    udp_indexer_.DestroyIndex(handle);
  } else {
    udp_sockets_lock_.unlock();
  }
}

std::shared_ptr<TcpSocket> NetResMgr::GetTcpSocket(TcpHandle handle) {
  std::lock_guard<std::mutex> lock(tcp_sockets_lock_);
  auto socket = tcp_sockets_.find(handle);
  if (socket == tcp_sockets_.end()) {
    LOG(kError, "can not find tcp handle: %u.", handle);
    return nullptr;
  }
  return socket->second;
}

std::shared_ptr<UdpSocket> NetResMgr::GetUdpSocket(UdpHandle handle) {
  std::lock_guard<std::mutex> lock(udp_sockets_lock_);
  auto socket = udp_sockets_.find(handle);
  if (socket == udp_sockets_.end()) {
    LOG(kError, "can not find udp handle: %u.", handle);
    return nullptr;
  }
  return socket->second;
}

TcpAcceptBuffer* NetResMgr::GetTcpAcceptBuffer() {
  auto buffer = new TcpAcceptBuffer;
  return buffer;
}

TcpSendBuffer* NetResMgr::GetTcpSendBuffer() {
  auto buffer = new TcpSendBuffer;
  return buffer;
}

TcpRecvBuffer* NetResMgr::GetTcpRecvBuffer() {
  auto buffer = new TcpRecvBuffer;
  return buffer;
}

UdpSendBuffer* NetResMgr::GetUdpSendBuffer() {
  auto buffer = new UdpSendBuffer;
  return buffer;
}

UdpRecvBuffer* NetResMgr::GetUdpRecvBuffer() {
  auto buffer = new UdpRecvBuffer;
  return buffer;
}

void NetResMgr::ReturnTcpAcceptBuffer(TcpAcceptBuffer* buffer) {
  if (buffer != nullptr) {
    delete buffer;
  }
}

void NetResMgr::ReturnTcpSendBuffer(TcpSendBuffer* buffer) {
  if (buffer != nullptr) {
    delete buffer;
  }
}

void NetResMgr::ReturnTcpRecvBuffer(TcpRecvBuffer* buffer) {
  if (buffer != nullptr) {
    delete buffer;
  }
}

void NetResMgr::ReturnUdpSendBuffer(UdpSendBuffer* buffer) {
  if (buffer != nullptr) {
    delete buffer;
  }
}

void NetResMgr::ReturnUdpRecvBuffer(UdpRecvBuffer* buffer) {
  if (buffer != nullptr) {
    delete buffer;
  }
}

bool NetResMgr::AsyncTcpAccept(TcpHandle handle, const std::shared_ptr<TcpSocket>& socket, TcpAcceptBuffer* buffer) {
  std::unique_ptr<TcpSocket> accept_socket(new TcpSocket);
  if (!accept_socket->Create(socket->callback())) {
    ReturnTcpAcceptBuffer(buffer);
    return false;
  }
  auto async_sock = accept_socket->socket();
  buffer->set_handle(handle);
  buffer->set_accept_socket(std::move(accept_socket));
  if (!socket->AsyncAccept(async_sock, buffer->buffer(), buffer->buffer_size(), buffer->ovlp())) {
    ReturnTcpAcceptBuffer(buffer);
    return false;
  }
  return true;
}

bool NetResMgr::AsyncTcpRecv(TcpHandle handle, const std::shared_ptr<TcpSocket>& socket, TcpRecvBuffer* buffer) {
  buffer->set_handle(handle);
  if (!socket->AsyncRecv(buffer->buffer(), buffer->buffer_size(), buffer->ovlp())) {
    ReturnTcpRecvBuffer(buffer);
    return false;
  }
  return true;
}

bool NetResMgr::AsyncUdpRecv(UdpHandle handle, const std::shared_ptr<UdpSocket>& socket, UdpRecvBuffer* buffer) {
  buffer->set_handle(handle);
  if (!socket->AsyncRecvFrom(buffer->buffer(), buffer->buffer_size(), buffer->from_addr(), buffer->addr_size(), buffer->ovlp())) {
    ReturnUdpRecvBuffer(buffer);
    return false;
  }
  return true;
}

bool NetResMgr::TransferAsyncType(LPOVERLAPPED ovlp, DWORD transfer_size) {
  auto async_buffer = (BaseBuffer*)ovlp;
  switch (async_buffer->async_type()) {
  case kAsyncTypeTcpAccept:
    return OnTcpAccept((TcpAcceptBuffer*)async_buffer);
  case kAsyncTypeTcpSend:
    return OnTcpSend((TcpSendBuffer*)async_buffer);
  case kAsyncTypeTcpRecv:
    return OnTcpRecv((TcpRecvBuffer*)async_buffer, transfer_size);
  case kAsyncTypeUdpSend:
    return OnUdpSend((UdpSendBuffer*)async_buffer);
  case kAsyncTypeUdpRecv:
    return OnUdpRecv((UdpRecvBuffer*)async_buffer, transfer_size);
  default:
    return false;
  }
}

bool NetResMgr::OnTcpAccept(TcpAcceptBuffer* buffer) {
  std::shared_ptr<TcpSocket> accept_socket(buffer->accept_socket());
  auto listen_handle = buffer->handle();
  auto listen_socket = GetTcpSocket(listen_handle);
  if (listen_socket == nullptr) {
    ReturnTcpAcceptBuffer(buffer);
    return true;
  }
  OnTcpAccept(listen_handle, listen_socket, accept_socket);
  buffer->ResetBuffer();
  if (!AsyncTcpAccept(listen_handle, listen_socket, buffer)) {
    OnTcpError(listen_handle, listen_socket->callback(), 1);
    return false;
  }
  return true;
}

bool NetResMgr::OnTcpSend(TcpSendBuffer* buffer) {
  ReturnTcpSendBuffer(buffer);
  return true;
}

bool NetResMgr::OnTcpRecv(TcpRecvBuffer* buffer, int size) {
  auto recv_handle = buffer->handle();
  auto recv_socket = GetTcpSocket(recv_handle);
  if (recv_socket == nullptr) {
    ReturnTcpRecvBuffer(buffer);
    return true;
  }
  auto callback = recv_socket->callback();
  if (size == 0) {
    ReturnTcpRecvBuffer(buffer);
    if (callback != nullptr) {
      callback->OnTcpDisconnected(recv_handle);
    }
    RemoveTcpSocket(recv_handle);
    return true;
  }
  auto recv_buff = buffer->buffer();
  if (!recv_socket->OnRecv(recv_buff, size)) {
    ReturnTcpRecvBuffer(buffer);
    OnTcpError(recv_handle, callback, 3);
    return false;
  }
  auto all_packets = recv_socket->all_packets();
  if (callback != nullptr) {
    for (const auto& i : all_packets) {
      callback->OnTcpReceived(recv_handle, i->packet(), i->size());
    }
  }
  buffer->ResetBuffer();
  if (!AsyncTcpRecv(recv_handle, recv_socket, buffer)) {
    OnTcpError(recv_handle, callback, 4);
    return false;
  }
  return true;
}

bool NetResMgr::OnUdpSend(UdpSendBuffer* buffer) {
  ReturnUdpSendBuffer(buffer);
  return true;
}

bool NetResMgr::OnUdpRecv(UdpRecvBuffer* buffer, int size) {
  auto recv_handle = buffer->handle();
  auto recv_socket = GetUdpSocket(recv_handle);
  if (recv_socket != nullptr) {
    ReturnUdpRecvBuffer(buffer);
    return true;
  }
  std::string ip;
  int port = 0;
  utility::FromSockAddr(*buffer->from_addr(), ip, port);
  auto callback = recv_socket->callback();
  if (callback != nullptr) {
    callback->OnUdpReceived(recv_handle, buffer->buffer(), size, ip, port);
  }
  buffer->ResetBuffer();
  if (!AsyncUdpRecv(recv_handle, recv_socket, buffer)) {
    OnUdpError(recv_handle, callback, 1);
    return false;
  }
  return true;
}

bool NetResMgr::OnTcpAccept(TcpHandle listen_handle, const std::shared_ptr<TcpSocket>& listen_socket, const std::shared_ptr<TcpSocket>& accept_socket) {
  auto accept_handle = kInvalidTcpHandle;
  if (!AddTcpSocket(accept_socket, accept_handle)) {
    return false;
  }
  if (!accept_socket->SetAccepted(listen_socket->socket())) {
    RemoveTcpSocket(accept_handle);
    return false;
  }
  if (!iocp_.BindToIOCP(accept_socket->socket())) {
    RemoveTcpSocket(accept_handle);
    return false;
  }
  auto callback = accept_socket->callback();
  if (callback != nullptr) {
    callback->OnTcpAccepted(listen_handle, accept_handle);
  }
  auto recv_buffer = GetTcpRecvBuffer();
  if (recv_buffer == nullptr) {
    OnTcpError(accept_handle, callback, 2);
    return false;
  }
  if (!AsyncTcpRecv(accept_handle, accept_socket, recv_buffer)) {
    OnTcpError(accept_handle, callback, 4);
    return false;
  }
  return true;
}

void NetResMgr::OnTcpError(TcpHandle handle, const std::shared_ptr<NetInterface>& callback, int error) {
  LOG(kError, "tcp handle %u error: %d.", handle, error);
  if (callback != nullptr) {
    callback->OnTcpError(handle, error);
  }
  RemoveTcpSocket(handle);
}

void NetResMgr::OnUdpError(UdpHandle handle, const std::shared_ptr<NetInterface>& callback, int error) {
  LOG(kError, "udp handle %u error: %d.", handle, error);
  if (callback != nullptr) {
    callback->OnUdpError(handle, error);
  }
  RemoveUdpSocket(handle);
}

} // namespace net