// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <thread>

#include "lib/bounded_queue.h"

struct ImageJob {
  uint8_t bus;
  uint8_t address;
};

class Imager {
 public:
  explicit Imager(std::string image_path);
  ~Imager();

  Imager(const Imager&) = delete;
  Imager& operator=(const Imager&) = delete;

  bool Submit(ImageJob job);

 private:
  std::string image_path_;
  BoundedQueue<ImageJob> jobs_{16};
  std::vector<std::thread> job_threads_;
  std::thread dispatcher_;

  void Run();
  void RunJob(ImageJob job);
};