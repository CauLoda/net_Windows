#ifndef NET_IOCP_H_
#define NET_IOCP_H_

#include "uncopyable.h"
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <WinSock2.h>

namespace net {

class IOCP : public utility::Uncopyable {
 public:
  IOCP();
  ~IOCP();
  bool Init(std::function<bool (LPOVERLAPPED, DWORD)>&& callback);
  void Uninit();
  bool BindToIOCP(SOCKET socket);

 private:
  bool ThreadWorker();

 private:
  bool init_;
  HANDLE iocp_;
  std::function<bool (LPOVERLAPPED, DWORD)> callback_;
  std::vector<std::unique_ptr<std::thread>> iocp_thread_;
};

} // namespace net

#endif	// NET_IOCP_H_