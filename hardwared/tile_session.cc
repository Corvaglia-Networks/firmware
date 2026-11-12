// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#include "tile_session.h"

#include <chrono>
#include <cstdint>

#include "absl/log/log.h"
#include "lib/tile_protocol.h"

constexpr int kInterface = 0;
constexpr unsigned char kEndpointIn = 0x81;
constexpr unsigned int kHeartbeatTimeoutMs = 3000;

TileSession::TileSession(libusb_device* dev)
    : dev_(libusb_ref_device(dev)),
      bus_(libusb_get_bus_number(dev)),
      address_(libusb_get_device_address(dev)) {}

TileSession::~TileSession() {
  stop_ = true;
  if (reader_.joinable()) {
    reader_.join();
  }
  if (handle_) {
    libusb_release_interface(handle_, kInterface);
    libusb_close(handle_);
  }
  libusb_unref_device(dev_);
}

bool TileSession::Start() {
  int r = libusb_open(dev_, &handle_);
  if (r != LIBUSB_SUCCESS) {
    LOG(ERROR) << "Tile " << int{bus_} << ":" << int{address_}
               << " open failed: " << libusb_error_name(r);
    handle_ = nullptr;
    return false;
  }

  r = libusb_claim_interface(handle_, kInterface);
  if (r != LIBUSB_SUCCESS) {
    LOG(ERROR) << "Tile " << int{bus_} << ":" << int{address_}
               << " claim failed: " << libusb_error_name(r);
    libusb_close(handle_);
    handle_ = nullptr;
    return false;
  }

  reader_ = std::thread(&TileSession::Run, this);
  return true;
}

void TileSession::Run() {
  unsigned char buf[64];
  bool alive = true;
  uint64_t heartbeats = 0;
  auto next_report = std::chrono::steady_clock::now();

  while (!stop_) {
    int n = 0;
    int r = libusb_bulk_transfer(handle_, kEndpointIn, buf, sizeof(buf), &n,
                                 kHeartbeatTimeoutMs);

    if (r == LIBUSB_ERROR_TIMEOUT) {
      if (alive) {
        LOG(WARNING) << "Tile " << int{bus_} << ":" << int{address_}
                     << " heartbeat lost";
        alive = false;
      }
      continue;
    }
    if (r != LIBUSB_SUCCESS) {
      LOG(INFO) << "Tile " << int{bus_} << ":" << int{address_}
                << " reader stopping: " << libusb_error_name(r);
      break;
    }

    if (n < static_cast<int>(sizeof(tile_packet))) continue;
    const auto* pkt = reinterpret_cast<const tile_packet*>(buf);
    if (pkt->magic != TILE_MAGIC) continue;

    if (pkt->type == TILE_HEARTBEAT) {
      ++heartbeats;
      const auto now = std::chrono::steady_clock::now();
      if (now >= next_report) {
        next_report = now + std::chrono::seconds(10);
        LOG(INFO) << "Tile " << int{bus_} << ":" << int{address_} << " alive, "
                  << heartbeats << " heartbeats";
      }
      if (!alive) {
        LOG(INFO) << "Tile " << int{bus_} << ":" << int{address_}
                  << " heartbeat recovered";
        alive = true;
      }
    }
  }
}