#pragma once

#include <sys/types.h>

#include <cstddef>
#include <optional>
#include <string>

class Connection {
 public:
  explicit Connection(int fd);
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  ssize_t Read(void* buf, size_t size);

  std::optional<std::string> ReadLine(size_t max_len = 256);

  bool WriteAll(const void* buf, size_t size);

  bool TryWriteAll(const void* buf, size_t size);

  void Shutdown();

 private:
  const int fd_;
};