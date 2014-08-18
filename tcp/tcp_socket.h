#ifndef NET_TCP_SOCKET_H_
#define NET_TCP_SOCKET_H_

#include "uncopyable.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <WinSock2.h>

namespace net {

class NetInterface;
class TcpHead;

class RecvPacket {
 public:
  RecvPacket() : packet_(nullptr), size_(0) {}
  ~RecvPacket() { if (deleter_ != nullptr) deleter_(packet_); }

  const char* packet() const { return packet_; }
  int size() const { return size_; }

 private:
  friend class TcpSocket;
  char* packet_;
  int size_;
  std::function<void (char*)> deleter_;
};

class TcpSocket : public utility::Uncopyable {
 public:
  TcpSocket();
  ~TcpSocket();

  bool Create(const std::weak_ptr<NetInterface>& callback);
  void Destroy();
  bool Bind(const std::string& ip, int port);
  bool Listen(int backlog);
  bool Connect(const std::string& ip, int port);
  bool AsyncAccept(SOCKET accept_sock, char* buffer, int size, LPOVERLAPPED ovlp);
  bool AsyncSend(const TcpHead* head, const char* buffer, int size, LPOVERLAPPED ovlp);
  bool AsyncRecv(char* buffer, int size, LPOVERLAPPED ovlp);
  bool SetAccepted(SOCKET listen_sock);
  bool GetLocalAddr(std::string& ip, int& port);
  bool GetRemoteAddr(std::string& ip, int& port);

  SOCKET socket() const { return socket_; }
  std::shared_ptr<NetInterface> callback() const { return callback_.lock(); }
  bool OnRecv(const char* data, int size);
  std::vector<std::unique_ptr<RecvPacket>> all_packets() { return std::move(all_packets_); }

 private:
  void ResetMember();
  bool ParseTcpHead(const char* data, int size, int& parsed_size);
  int ParseTcpPacket(const char* data, int size);
  bool ResetCurrentPacket();
  void ResetCurrentHead();

 private:
  std::weak_ptr<NetInterface> callback_;
  SOCKET socket_;
  bool bind_;
  bool listen_;
  bool connect_;
  std::vector<char> current_head_;
  std::unique_ptr<RecvPacket> current_packet_;
  int current_packet_offset_;
  std::vector<std::unique_ptr<RecvPacket>> all_packets_;
};

} // namespace net

#endif	// NET_TCP_SOCKET_H_