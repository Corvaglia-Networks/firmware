#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template <typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

  BoundedQueue(const BoundedQueue&) = delete;
  BoundedQueue& operator=(const BoundedQueue&) = delete;

  bool TryPush(T item) {
    {
      std::lock_guard lock(mutex_);
      if (closed_ || queue_.size() >= capacity_) return false;
      queue_.push(std::move(item));
    }
    not_empty_.notify_one();
    return true;
  }

  std::optional<T> Pop() {
    std::unique_lock lock(mutex_);
    not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) return std::nullopt;
    std::optional<T> item(std::move(queue_.front()));
    queue_.pop();
    return item;
  }

  void Close() {
    {
      std::lock_guard lock(mutex_);
      closed_ = true;
    }
    not_empty_.notify_all();
  }

 private:
  const std::size_t capacity_;
  std::mutex mutex_;
  std::condition_variable not_empty_;
  std::queue<T> queue_;
  bool closed_ = false;
};