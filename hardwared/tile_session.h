#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "libusb.h"

class TileSession {
 public:
  explicit TileSession(libusb_device* dev);
  ~TileSession();

  TileSession(const TileSession&) = delete;
  TileSession& operator=(const TileSession&) = delete;

  bool Start();

  uint8_t Bus() const { return bus_; }
  uint8_t Address() const { return address_; }

 private:
  void Run();

  libusb_device* dev_;
  libusb_device_handle* handle_ = nullptr;
  const uint8_t bus_;
  const uint8_t address_;

  std::atomic<bool> stop_{false};
  std::thread reader_;
};