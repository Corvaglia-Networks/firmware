// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>

#include "lib/connection.h"

class SocketServer {
 public:
  using Handler = std::function<void(Connection&)>;

  SocketServer(std::string path, Handler handler);
  ~SocketServer();

  SocketServer(const SocketServer&) = delete;
  SocketServer& operator=(const SocketServer&) = delete;

  bool Start();

  void Stop();

 private:
  struct Client {
    std::unique_ptr<Connection> conn;
    std::thread thread;
    std::atomic<bool> done{false};
  };

  void AcceptLoop();
  void ReapFinished();

  const std::string path_;
  const Handler handler_;

  int listen_fd_ = -1;
  int wake_fds_[2] = {-1, -1};
  std::atomic<bool> stopping_{false};

  std::list<std::unique_ptr<Client>> clients_;

  std::thread accept_thread_;
};