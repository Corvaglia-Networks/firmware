#include "supervisor.h"

#include <sys/time.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/log/log.h"

Supervisor::Supervisor(std::string image_path)
    : image_path_(std::move(image_path)),
      imager_(image_path_),
      console_server_("/tmp/hardwared.sock",
                      [this](Connection& conn) { HandleConsoleClient(conn); }) {
  for (int i = 0; i < 8; i++) {
    ports_.push_back(std::make_unique<LoopbackPort>());
  }
}

Supervisor::~Supervisor() {
  sessions_.clear();
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

  if (!console_server_.Start()) {
    return 1;
  }

  while (true) {
    timeval tv = {1, 0};
    libusb_handle_events_timeout_completed(ctx_, &tv, nullptr);

    std::vector<libusb_device*> batch;
    std::vector<std::pair<uint8_t, uint8_t>> gone;
    {
      std::lock_guard lock(arrivals_mutex_);
      batch.swap(arrivals_);
      gone.swap(departures_);
    }
    for (const auto& [bus, address] : gone) {
      HandleDeparture(bus, address);
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

  auto* self = static_cast<Supervisor*>(user_data);
  const uint8_t bus = libusb_get_bus_number(dev);
  const uint8_t address = libusb_get_device_address(dev);

  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
    if (IsTile(desc)) {
      std::lock_guard lock(self->arrivals_mutex_);
      self->departures_.emplace_back(bus, address);
    } else {
      LOG(INFO) << "PICOBOOT device left at " << int{bus} << ":"
                << int{address};
    }
    return 0;
  }

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
    auto session = std::make_unique<TileSession>(dev);
    if (session->Start()) {
      sessions_.push_back(std::move(session));
    }
  }

  libusb_unref_device(dev);
}

void Supervisor::HandleDeparture(uint8_t bus, uint8_t address) {
  const auto old_size = sessions_.size();
  sessions_.erase(
      std::remove_if(sessions_.begin(), sessions_.end(),
                     [bus, address](const std::unique_ptr<TileSession>& s) {
                       return s->Bus() == bus && s->Address() == address;
                     }),
      sessions_.end());
  LOG(INFO) << "Tile left at " << int{bus} << ":" << int{address}
            << (sessions_.size() < old_size ? ", session removed"
                                            : ", no session to remove");
}

Port* Supervisor::FindPort(int port_number) {
  if (port_number < 1 || port_number > static_cast<int>(ports_.size())) {
    return nullptr;
  }
  return ports_[port_number - 1].get();
}

void Supervisor::HandleConsoleClient(Connection& conn) {
  const auto line = conn.ReadLine();
  if (!line) return;

  constexpr std::string_view kAttach = "attach ";
  const std::string_view cmd = *line;
  int port_number = -1;
  if (cmd.substr(0, kAttach.size()) != kAttach ||
      std::from_chars(cmd.data() + kAttach.size(), cmd.data() + cmd.size(),
                      port_number)
              .ec != std::errc()) {
    constexpr std::string_view kError = "error expected: attach <port>\n";
    conn.WriteAll(kError.data(), kError.size());
    return;
  }

  Port* port = FindPort(port_number);
  if (port == nullptr) {
    constexpr std::string_view kError = "error no such port\n";
    conn.WriteAll(kError.data(), kError.size());
    return;
  }

  constexpr std::string_view kOk = "ok\n";
  conn.WriteAll(kOk.data(), kOk.size());
  LOG(INFO) << "Client attached to port " << port_number;

  const int subscription =
      port->Subscribe([&conn](const uint8_t* data, size_t size) {
        if (!conn.TryWriteAll(data, size)) conn.Shutdown();
      });

  uint8_t buf[256];
  ssize_t n;
  while ((n = conn.Read(buf, sizeof(buf))) > 0) {
    port->Write(buf, static_cast<size_t>(n));
  }

  port->Unsubscribe(subscription);
  LOG(INFO) << "Client detached from port " << port_number;
}