// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#include "imager.h"

#include <spawn.h>
#include <sys/wait.h>

#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "absl/log/log.h"

extern char** environ;

Imager::Imager(std::string image_path) : image_path_(std::move(image_path)) {
  dispatcher_ = std::thread(&Imager::Run, this);
}

Imager::~Imager() {
  jobs_.Close();
  dispatcher_.join();
}

bool Imager::Submit(ImageJob job) { return jobs_.TryPush(job); }

void Imager::Run() {
  while (true) {
    auto job = jobs_.Pop();

    if (!job.has_value()) {
      break;
    }
    RunJob(job.value());
  }
}

void Imager::RunJob(ImageJob job) {
  const std::string bus = std::to_string(job.bus);
  const std::string address = std::to_string(job.address);
  const char* argv[] = {"picotool",          "load",          "-x",
                        image_path_.c_str(), "--bus",         bus.c_str(),
                        "--address",         address.c_str(), nullptr};
  pid_t pid = 0;
  int rc = posix_spawnp(&pid, "picotool", nullptr, nullptr,
                        const_cast<char* const*>(argv), environ);
  if (rc != 0) {
    LOG(ERROR) << "Failed to spawn picotool: " << strerror(rc);
    return;
  }
  int status = 0;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
      WEXITSTATUS(status) != 0) {
    LOG(ERROR) << "Firmware load failed on " << bus << ":" << address;
    return;
  }
  LOG(INFO) << "Firmware loaded successfully on " << bus << ":" << address;
}