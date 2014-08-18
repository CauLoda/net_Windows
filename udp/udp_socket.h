#ifndef NET_UDP_SOCKET_H_
#define NET_UDP_SOCKET_H_

#include "uncopyable.h"
#include <memory>
#include <string>
#include <WinSock2.h>

namespace net {

class NetInterface;

class UdpSocket : public utility::Uncopyable {
 public:
  UdpSocket();
  ~UdpSocket();

  bool Create(const std::weak_ptr<NetInterface>& callback);
  bool Bind(const std::string& ip, int port);
  void Destroy();
  bool AsyncSendTo(const char* buffer, int size, const std::string& ip, int port, LPOVERLAPPED ovlp);
  bool AsyncRecvFrom(char* buffer, int size, PSOCKADDR_IN addr, PINT addr_size, LPOVERLAPPED ovlp);

  SOCKET socket() const { return socket_; }
  std::shared_ptr<NetInterface> callback() const { return callback_.lock(); }

 private:
  void ResetMember();

 private:
  std::weak_ptr<NetInterface> callback_;
  SOCKET socket_;
  bool bind_;
};

} // namespace net

#endif	// NET_UDP_SOCKET_H_