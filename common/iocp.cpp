#include "iocp.h"
#include "log.h"
#include "utility.h"

namespace net {

IOCP::IOCP() {
  init_ = false;
  iocp_ = NULL;
}

IOCP::~IOCP() {
  Uninit();
}

bool IOCP::Init(std::function<bool (LPOVERLAPPED, DWORD)>&& callback) {
  if (init_) {
    return true;
  }
  if (callback == nullptr) {
    LOG(kStartup, "initialize IOCP failed: invalid callback parameter.");
    return false;
  }
  WSAData wsa_data = {0};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    LOG(kStartup, "WSAStartup failed, error code: %d.", WSAGetLastError());
    return false;
  }
  callback_ = std::move(callback);
  init_ = true;
  iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
  if (iocp_ == NULL) {
    LOG(kStartup, "CreateIoCompletionPort failed, error code: %d.", WSAGetLastError());
    Uninit();
    return false;
  }
  auto thread_num = utility::GetProcessorNum() * 2;
  auto thread_proc = std::bind(&IOCP::ThreadWorker, this);
  for (auto i = 0; i < thread_num; ++i) {
    iocp_thread_.push_back(std::make_unique<std::thread>(thread_proc));
  }
  return true;
}

void IOCP::Uninit() {
  if (!init_) {
    return;
  }
  for (const auto& i : iocp_thread_) {
    PostQueuedCompletionStatus(iocp_, 0, NULL, NULL);
  }
  for (const auto& i : iocp_thread_) {
    i->join();
  }
  iocp_thread_.clear();
  if (iocp_ != NULL) {
    CloseHandle(iocp_);
    iocp_ = NULL;
  }
  WSACleanup();
  callback_ = nullptr;
  init_ = false;
}

bool IOCP::BindToIOCP(SOCKET socket) {
  if (socket == INVALID_SOCKET) {
    LOG(kError, "BindToIOCP failed: invalid socket parameter.");
    return false;
  }
  auto existing_iocp = CreateIoCompletionPort((HANDLE)socket, iocp_, NULL, 0);
  if (existing_iocp != iocp_) {
    LOG(kError, "BindToIOCP failed, error code: %d.", WSAGetLastError());
    return false;
  }
  return true;
}

bool IOCP::ThreadWorker() {
  while (true) {
    DWORD transfer_size = 0;
    ULONG completion_key = NULL;
    LPOVERLAPPED ovlp = NULL;
    if (!GetQueuedCompletionStatus(iocp_, &transfer_size, &completion_key, &ovlp, INFINITE)) {
      auto error_code = WSAGetLastError();
      if (error_code != ERROR_NETNAME_DELETED && error_code != ERROR_CONNECTION_ABORTED &&
        error_code != ERROR_OPERATION_ABORTED) {
        LOG(kError, "GetQueuedCompletionStatus failed, error code: %d.", error_code);
      }
    }
    if (transfer_size == 0 && ovlp == NULL) {
      break;
    }
    if (callback_ != nullptr) {
      callback_(ovlp, transfer_size);
    }
  }
  return true;
}

} // namespace net