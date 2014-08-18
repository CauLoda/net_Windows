#ifndef NET_RES_MANAGER_H_
#define NET_RES_MANAGER_H_

#include "indexer.h"
#include "iocp.h"
#include "net_interface.h"
#include "tcp_buffer.h"
#include "tcp_socket.h"
#include "udp_buffer.h"
#include "udp_socket.h"
#include "singleton.h"
#include "uncopyable.h"
#include <unordered_map>

namespace net {

class NetResMgr : public utility::Uncopyable {
 public:
  NetResMgr();
  ~NetResMgr();

  bool StartupNet();
  bool CleanupNet();
  bool TcpCreate(const std::weak_ptr<NetInterface>& callback, const std::string& ip, int port, TcpHandle& new_handle);
  bool TcpDestroy(TcpHandle handle);
  bool TcpListen(TcpHandle handle);
  bool TcpConnect(TcpHandle handle, const std::string& ip, int port);
  bool TcpSend(TcpHandle handle, std::unique_ptr<char[]>&& packet, int size);
  bool TcpGetLocalAddr(TcpHandle handle, char ip[16], int& port);
  bool TcpGetRemoteAddr(TcpHandle handle, char ip[16], int& port);
  bool UdpCreate(const std::weak_ptr<NetInterface>& callback, const std::string& ip, int port, UdpHandle& new_handle);
  bool UdpDestroy(UdpHandle handle);
  bool UdpSendTo(UdpHandle handle, std::unique_ptr<char[]>&& packet, int size, const std::string& ip, int port);

 private:
  bool AddTcpSocket(const std::shared_ptr<TcpSocket>& new_socket, TcpHandle& new_handle);
  bool AddUdpSocket(const std::shared_ptr<UdpSocket>& new_socket, TcpHandle& new_handle);
  void RemoveTcpSocket(TcpHandle handle);
  void RemoveUdpSocket(UdpHandle handle);
  std::shared_ptr<TcpSocket> GetTcpSocket(TcpHandle handle);
  std::shared_ptr<UdpSocket> GetUdpSocket(UdpHandle handle);

  TcpAcceptBuffer* GetTcpAcceptBuffer();
  TcpSendBuffer* GetTcpSendBuffer();
  TcpRecvBuffer* GetTcpRecvBuffer();
  UdpSendBuffer* GetUdpSendBuffer();
  UdpRecvBuffer* GetUdpRecvBuffer();
  void ReturnTcpAcceptBuffer(TcpAcceptBuffer* buffer);
  void ReturnTcpSendBuffer(TcpSendBuffer* buffer);
  void ReturnTcpRecvBuffer(TcpRecvBuffer* buffer);
  void ReturnUdpSendBuffer(UdpSendBuffer* buffer);
  void ReturnUdpRecvBuffer(UdpRecvBuffer* buffer);

  bool AsyncTcpAccept(TcpHandle handle, const std::shared_ptr<TcpSocket>& socket, TcpAcceptBuffer* buffer);
  bool AsyncTcpRecv(TcpHandle handle, const std::shared_ptr<TcpSocket>& socket, TcpRecvBuffer* buffer);
  bool AsyncUdpRecv(UdpHandle handle, const std::shared_ptr<UdpSocket>& socket, UdpRecvBuffer* buffer);

  bool TransferAsyncType(LPOVERLAPPED ovlp, DWORD transfer_size);
  bool OnTcpAccept(TcpAcceptBuffer* buffer);
  bool OnTcpSend(TcpSendBuffer* buffer);
  bool OnTcpRecv(TcpRecvBuffer* buffer, int size);
  bool OnUdpSend(UdpSendBuffer* buffer);
  bool OnUdpRecv(UdpRecvBuffer* buffer, int size);

  bool OnTcpAccept(TcpHandle listen_handle, const std::shared_ptr<TcpSocket>& listen_socket, const std::shared_ptr<TcpSocket>& accept_socket);
  void OnTcpError(TcpHandle handle, const std::shared_ptr<NetInterface>& callback, int error);
  void OnUdpError(UdpHandle handle, const std::shared_ptr<NetInterface>& callback, int error);

 private:
  bool net_started_;
  IOCP iocp_;
  utility::Indexer tcp_indexer_;
  utility::Indexer udp_indexer_;
  std::unordered_map<TcpHandle, std::shared_ptr<TcpSocket>> tcp_sockets_;
  std::unordered_map<UdpHandle, std::shared_ptr<UdpSocket>> udp_sockets_;
  std::mutex tcp_sockets_lock_;
  std::mutex udp_sockets_lock_;
};

typedef utility::Singleton<NetResMgr> SingleNetResMgr;

} // namespace net

#endif	// NET_RES_MANAGER_H_