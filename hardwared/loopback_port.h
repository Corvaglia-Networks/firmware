#pragma once

#include <map>
#include <mutex>
#include <utility>

#include "port.h"

class LoopbackPort : public Port {
 public:
  bool Write(const uint8_t* data, size_t size) override {
    std::lock_guard lock(mutex_);
    for (auto& [id, sink] : sinks_) sink(data, size);
    return true;
  }

  int Subscribe(Sink sink) override {
    std::lock_guard lock(mutex_);
    sinks_.emplace(next_id_, std::move(sink));
    return next_id_++;
  }

  void Unsubscribe(int id) override {
    std::lock_guard lock(mutex_);
    sinks_.erase(id);
  }

 private:
  std::mutex mutex_;
  std::map<int, Sink> sinks_;
  int next_id_ = 0;
};