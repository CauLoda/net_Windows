#ifndef TCP_SOCKET_H_
#define TCP_SOCKET_H_

#include "../utility/uncopyable.h"
#include <string>
#include <WinSock2.h>

namespace tcp {

class Socket : public utility::Uncopyable {
 public:
  ~Socket();
  bool Create();
  bool Bind(const std::string& ip, int port);
  bool Listen(int backlog);
  bool Connect(const std::string& ip, int port, int timeout);
  bool AsyncAccept(SOCKET accept_sock, char* buffer, int size, LPOVERLAPPED ovlp);
  bool AsyncSend(const char* buffer, int size, LPOVERLAPPED ovlp);
  bool AsyncRecv(char* buffer, int size, LPOVERLAPPED ovlp);
  bool SetAccepted(SOCKET listen_sock);
  bool GetLocalAddr(std::string& ip, int& port);
  bool GetRemoteAddr(std::string& ip, int& port);

  SOCKET socket() { return socket_; }

 private:
  bool SetNonblockingMode(bool nonblocking);

 private:
  SOCKET socket_ = INVALID_SOCKET;
};

} // namespace tcp

#endif	// TCP_SOCKET_H_