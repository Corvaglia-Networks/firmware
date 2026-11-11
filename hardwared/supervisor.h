#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "imager.h"
#include "lib/tile_protocol.h"
#include "libusb.h"
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

  const std::string image_path_;
  libusb_context* ctx_ = nullptr;

  std::mutex arrivals_mutex_;
  std::vector<libusb_device*> arrivals_;

  Imager imager_;
};