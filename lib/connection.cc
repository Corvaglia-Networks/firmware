// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#include "connection.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>

namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

void DisableSigpipe(int fd) {
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)fd;
#endif
}

}  // namespace

Connection::Connection(int fd) : fd_(fd) { DisableSigpipe(fd_); }

Connection::~Connection() { close(fd_); }

ssize_t Connection::Read(void* buf, size_t size) {
  for (;;) {
    const ssize_t n = recv(fd_, buf, size, 0);
    if (n < 0 && errno == EINTR) continue;
    return n;
  }
}

std::optional<std::string> Connection::ReadLine(size_t max_len) {
  std::string line;
  while (line.size() <= max_len) {
    char c;
    if (Read(&c, 1) != 1) return std::nullopt;
    if (c == '\n') {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      return line;
    }
    line.push_back(c);
  }
  return std::nullopt;
}

bool Connection::WriteAll(const void* buf, size_t size) {
  const char* p = static_cast<const char*>(buf);
  while (size > 0) {
    const ssize_t n = send(fd_, p, size, kSendFlags);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    p += n;
    size -= static_cast<size_t>(n);
  }
  return true;
}

bool Connection::TryWriteAll(const void* buf, size_t size) {
  const ssize_t n = send(fd_, buf, size, kSendFlags | MSG_DONTWAIT);
  return n == static_cast<ssize_t>(size);
}

void Connection::Shutdown() { shutdown(fd_, SHUT_RDWR); }
