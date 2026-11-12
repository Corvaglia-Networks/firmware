// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "imager.h"
#include "lib/socket_server.h"
#include "lib/tile_protocol.h"
#include "libusb.h"
#include "loopback_port.h"
#include "tile_session.h"

constexpr uint16_t kPicobootVid = 0x2E8A;
constexpr uint16_t kPicobootPid = 0x000F;

class Supervisor {
 public:
  explicit Supervisor(std::string image_path);
  ~Supervisor();

  Supervisor(const Supervisor&) = delete;
  Supervisor& operator=(const Supervisor&) = delete;

  int Run();

 private:
  static int LIBUSB_CALL OnHotplug(libusb_context* ctx, libusb_device* dev,
                                   libusb_hotplug_event event, void* user_data);

  static bool IsTile(const libusb_device_descriptor& d) {
    return d.idVendor == TILE_USB_VID && d.idProduct == TILE_USB_PID;
  }

  static bool IsPicoboot(const libusb_device_descriptor& d) {
    return d.idVendor == kPicobootVid && d.idProduct == kPicobootPid;
  }

  void HandleArrival(libusb_device* dev);
  void HandleDeparture(uint8_t bus, uint8_t address);
  Port* FindPort(int port_number);
  void HandleConsoleClient(Connection& conn);

  const std::string image_path_;
  libusb_context* ctx_ = nullptr;

  std::mutex arrivals_mutex_;
  std::vector<libusb_device*> arrivals_;
  std::vector<std::pair<uint8_t, uint8_t>> departures_;

  std::vector<std::unique_ptr<TileSession>> sessions_;

  Imager imager_;
  std::vector<std::unique_ptr<LoopbackPort>> ports_;
  SocketServer console_server_;
};