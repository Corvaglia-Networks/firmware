// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#pragma once
#include <stdint.h>

#define TILE_MAGIC 0x454C4954

#define TILE_HEARTBEAT 0
#define TILE_RESET 1

#define TILE_USB_VID 0x1209
#define TILE_USB_PID 0x0001

struct tile_packet {
  uint32_t magic;
  uint8_t type;
  uint8_t length;
} __attribute__((packed));