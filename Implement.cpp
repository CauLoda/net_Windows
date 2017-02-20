#include "Interface/NetModule.h"
#include "net_res_mgr.h"

namespace net {

auto g_SingleNetResMgr = new NetResMgr;

bool StartupNet(NetInterface* callback) {
  return g_SingleNetResMgr->StartupNet(callback);
}

bool CleanupNet() {
  return g_SingleNetResMgr->CleanupNet();
}

bool TcpCreate(const char* ip, int port, TcpHandle& new_handle) {
  return g_SingleNetResMgr->TcpCreate(ip, port, new_handle);
}

bool TcpDestroy(TcpHandle handle) {
  return g_SingleNetResMgr->TcpDestroy(handle);
}

bool TcpListen(TcpHandle handle) {
  return g_SingleNetResMgr->TcpListen(handle);
}

bool TcpConnect(TcpHandle handle, const char* ip, int port, int timeout) {
  return g_SingleNetResMgr->TcpConnect(handle, ip, port, timeout);
}

bool TcpSend(TcpHandle handle, const char* packet, int size) {
  return g_SingleNetResMgr->TcpSend(handle, packet, size);
}

bool TcpGetLocalAddr(TcpHandle handle, char ip[16], int& port) {
  return g_SingleNetResMgr->TcpGetLocalAddr(handle, ip, port);
}

bool TcpGetRemoteAddr(TcpHandle handle, char ip[16], int& port) {
  return g_SingleNetResMgr->TcpGetRemoteAddr(handle, ip, port);
}

} // namespace net