#include "net_interface.h"
#include "net_res_mgr.h"

namespace net {

bool NetInterface::StartupNet() {
  return SingleNetResMgr::GetInstance()->StartupNet();
}

bool NetInterface::CleanupNet() {
  if (!SingleNetResMgr::GetInstance()->CleanupNet()) {
    return false;
  }
  SingleNetResMgr::Release();
  return true;
}

bool NetInterface::TcpCreate(const std::string& ip, int port, TcpHandle& new_handle) {
  return SingleNetResMgr::GetInstance()->TcpCreate(shared_from_this(), ip, port, new_handle);
}

bool NetInterface::TcpDestroy(TcpHandle handle) {
  return SingleNetResMgr::GetInstance()->TcpDestroy(handle);
}

bool NetInterface::TcpListen(TcpHandle handle) {
  return SingleNetResMgr::GetInstance()->TcpListen(handle);
}

bool NetInterface::TcpConnect(TcpHandle handle, const std::string& ip, int port) {
  return SingleNetResMgr::GetInstance()->TcpConnect(handle, ip, port);
}

bool NetInterface::TcpSend(TcpHandle handle, std::unique_ptr<char[]>&& packet, int size) {
  return SingleNetResMgr::GetInstance()->TcpSend(handle, std::move(packet), size);
}

bool NetInterface::TcpGetLocalAddr(TcpHandle handle, char ip[16], int& port) {
  return SingleNetResMgr::GetInstance()->TcpGetLocalAddr(handle, ip, port);
}

bool NetInterface::TcpGetRemoteAddr(TcpHandle handle, char ip[16], int& port) {
  return SingleNetResMgr::GetInstance()->TcpGetRemoteAddr(handle, ip, port);
}

bool NetInterface::UdpCreate(const std::string& ip, int port, UdpHandle& new_handle) {
  return SingleNetResMgr::GetInstance()->UdpCreate(shared_from_this(), ip, port, new_handle);
}

bool NetInterface::UdpDestroy(UdpHandle handle) {
  return SingleNetResMgr::GetInstance()->UdpDestroy(handle);
}

bool NetInterface::UdpSendTo(UdpHandle handle, std::unique_ptr<char[]>&& packet, int size, const std::string& ip, int port) {
  return SingleNetResMgr::GetInstance()->UdpSendTo(handle, std::move(packet), size, ip, port);
}

} // namespace net