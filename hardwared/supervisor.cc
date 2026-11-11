#include "supervisor.h"

#include <sys/time.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/log/log.h"

Supervisor::Supervisor(std::string image_path)
    : image_path_(std::move(image_path)), imager_(image_path_) {}

Supervisor::~Supervisor() {
  for (libusb_device* dev : arrivals_) {
    libusb_unref_device(dev);
  }
  if (ctx_ != nullptr) {
    libusb_exit(ctx_);
  }
}

int Supervisor::Run() {
  std::error_code ec;
  const auto image_size = std::filesystem::file_size(image_path_, ec);
  if (ec || image_size == 0) {
    LOG(ERROR) << "Cannot use firmware file " << image_path_ << ": "
               << (ec ? ec.message() : "file is empty");
    return 1;
  }
  LOG(INFO) << "Using firmware file " << image_path_ << " (" << image_size
            << " bytes)";

  if (libusb_init(&ctx_) != 0) {
    LOG(ERROR) << "libusb_init failed";
    return 1;
  }
  const libusb_version* v = libusb_get_version();
  LOG(INFO) << "libusb " << v->major << "." << v->minor << "." << v->micro;

  if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
    LOG(ERROR) << "This libusb has no hotplug support";
    return 1;
  }

  const int rc = libusb_hotplug_register_callback(
      ctx_,
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &Supervisor::OnHotplug, this, nullptr);
  if (rc != 0) {
    LOG(ERROR) << "Hotplug registration failed: " << libusb_error_name(rc);
    return 1;
  }

  while (true) {
    timeval tv = {1, 0};
    libusb_handle_events_timeout_completed(ctx_, &tv, nullptr);

    std::vector<libusb_device*> batch;
    {
      std::lock_guard lock(arrivals_mutex_);
      batch.swap(arrivals_);
    }
    for (libusb_device* dev : batch) {
      HandleArrival(dev);
    }
  }
}

int LIBUSB_CALL Supervisor::OnHotplug(libusb_context* ctx, libusb_device* dev,
                                      libusb_hotplug_event event,
                                      void* user_data) {
  libusb_device_descriptor desc;
  libusb_get_device_descriptor(dev, &desc);
  if (!IsTile(desc) && !IsPicoboot(desc)) return 0;

  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
    LOG(INFO) << (IsTile(desc) ? "Tile" : "PICOBOOT device") << " left at "
              << int{libusb_get_bus_number(dev)} << ":"
              << int{libusb_get_device_address(dev)};
    return 0;
  }

  auto* self = static_cast<Supervisor*>(user_data);
  std::lock_guard lock(self->arrivals_mutex_);
  self->arrivals_.push_back(libusb_ref_device(dev));
  return 0;
}

void Supervisor::HandleArrival(libusb_device* dev) {
  libusb_device_descriptor desc;
  libusb_get_device_descriptor(dev, &desc);
  const uint8_t bus = libusb_get_bus_number(dev);
  const uint8_t address = libusb_get_device_address(dev);

  if (IsPicoboot(desc)) {
    LOG(INFO) << "PICOBOOT device at " << int{bus} << ":" << int{address};
    if (!imager_.Submit({bus, address})) {
      LOG(ERROR) << "Imager queue full, skipping " << int{bus} << ":"
                 << int{address};
    }
  } else if (IsTile(desc)) {
    LOG(INFO) << "Tile at " << int{bus} << ":" << int{address};
    // TODO: Hand Tile to TileManager.
  }

  libusb_unref_device(dev);
}