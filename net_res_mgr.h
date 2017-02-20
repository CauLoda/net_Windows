#ifndef NET_RES_MANAGER_H_
#define NET_RES_MANAGER_H_

#include "Interface/NetModule.h"
#include "tcp/buffer.h"
#include "utility/indexer.h"
#include "utility/iocp.h"
#include <map>

namespace net {

typedef std::shared_ptr<tcp::Socket> TcpSocketPtr;

class NetResMgr : public utility::Uncopyable {
 public:
  bool StartupNet(NetInterface* callback);
  bool CleanupNet();
  bool TcpCreate(const std::string& ip, int port, TcpHandle& new_handle);
  bool TcpDestroy(TcpHandle handle);
  bool TcpListen(TcpHandle handle);
  bool TcpConnect(TcpHandle handle, const std::string& ip, int port, int timeout);
  bool TcpSend(TcpHandle handle, const char* packet, int size);
  bool TcpGetLocalAddr(TcpHandle handle, char ip[16], int& port);
  bool TcpGetRemoteAddr(TcpHandle handle, char ip[16], int& port);

 private:
  bool AddTcpSocket(const TcpSocketPtr& new_socket, TcpHandle& new_handle);
  void RemoveTcpSocket(TcpHandle handle);
  TcpSocketPtr GetTcpSocket(TcpHandle handle);

  bool AsyncTcpAccept(TcpHandle handle, const TcpSocketPtr& socket, std::unique_ptr<tcp::AcceptBuffer> buffer);
  bool AsyncTcpRecv(TcpHandle handle, const TcpSocketPtr& socket, std::unique_ptr<tcp::RecvBuffer> buffer);

  void IocpCallbackFunc(DWORD error_code, DWORD size, LPOVERLAPPED ovlp);
  void OnTcpAccept(std::unique_ptr<tcp::AcceptBuffer> buffer);
  void OnTcpSend(std::unique_ptr<tcp::SendBuffer> buffer);
  void OnTcpRecv(std::unique_ptr<tcp::RecvBuffer> buffer, int size);

 private:
  NetInterface* callback_ = nullptr;
  utility::Iocp iocp_;
  utility::Indexer tcp_indexer_;
  std::map<TcpHandle, TcpSocketPtr> tcp_sockets_;
  std::mutex tcp_sockets_lock_;
};

} // namespace net

#endif	// NET_RES_MANAGER_H_