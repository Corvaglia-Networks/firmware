// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#include "lib/socket_server.h"

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

#include "absl/log/log.h"

SocketServer::SocketServer(std::string path, Handler handler)
    : path_(std::move(path)), handler_(std::move(handler)) {}

SocketServer::~SocketServer() { Stop(); }

bool SocketServer::Start() {
  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  if (path_.size() >= sizeof(addr.sun_path)) {
    LOG(ERROR) << "Socket path too long: " << path_;
    return false;
  }
  std::memcpy(addr.sun_path, path_.c_str(), path_.size() + 1);

  if (pipe(wake_fds_) != 0) {
    LOG(ERROR) << "pipe failed: " << std::strerror(errno);
    return false;
  }

  listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    LOG(ERROR) << "socket failed: " << std::strerror(errno);
    return false;
  }

  unlink(path_.c_str());
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    LOG(ERROR) << "bind " << path_ << " failed: " << std::strerror(errno);
    return false;
  }
  chmod(path_.c_str(), 0660);

  if (listen(listen_fd_, 16) != 0) {
    LOG(ERROR) << "listen failed: " << std::strerror(errno);
    return false;
  }

  accept_thread_ = std::thread(&SocketServer::AcceptLoop, this);
  LOG(INFO) << "Listening on " << path_;
  return true;
}

void SocketServer::Stop() {
  if (stopping_.exchange(true)) return;

  if (accept_thread_.joinable()) {
    const char byte = 0;
    if (write(wake_fds_[1], &byte, 1) != 1) {
      LOG(ERROR) << "Could not wake accept loop: " << std::strerror(errno);
    }
    accept_thread_.join();
  }

  for (auto& client : clients_) {
    client->conn->Shutdown();
  }
  for (auto& client : clients_) {
    client->thread.join();
  }
  clients_.clear();

  if (listen_fd_ >= 0) {
    close(listen_fd_);
    unlink(path_.c_str());
  }
  if (wake_fds_[0] >= 0) close(wake_fds_[0]);
  if (wake_fds_[1] >= 0) close(wake_fds_[1]);
}

void SocketServer::AcceptLoop() {
  while (!stopping_) {
    pollfd fds[2] = {{listen_fd_, POLLIN, 0}, {wake_fds_[0], POLLIN, 0}};
    if (poll(fds, 2, -1) < 0) {
      if (errno == EINTR) continue;
      LOG(ERROR) << "poll failed: " << std::strerror(errno);
      return;
    }
    if (fds[1].revents != 0) return;
    if ((fds[0].revents & POLLIN) == 0) continue;

    const int fd = accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) continue;

    ReapFinished();

    auto client = std::make_unique<Client>();
    client->conn = std::make_unique<Connection>(fd);
    Client* c = client.get();
    clients_.push_back(std::move(client));
    c->thread = std::thread([this, c] {
      handler_(*c->conn);
      c->conn->Shutdown();
      c->done = true;
    });
  }
}

void SocketServer::ReapFinished() {
  for (auto it = clients_.begin(); it != clients_.end();) {
    if ((*it)->done) {
      (*it)->thread.join();
      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}