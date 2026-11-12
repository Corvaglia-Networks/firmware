// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#include <string.h>

#include "lib/tile_protocol.h"
#include "pico/stdlib.h"
#include "tusb.h"

int main() {
  gpio_init(22);
  gpio_set_dir(22, GPIO_OUT);

  gpio_init(23);

  tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO,
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  uint8_t buf[1024];
  uint8_t bytesRx = 0;

  absolute_time_t next_tick = make_timeout_time_ms(1000);
  absolute_time_t led_off = make_timeout_time_ms(1100);

  while (true) {
    tud_task();

    if (time_reached(next_tick)) {
      next_tick = make_timeout_time_ms(1000);
      gpio_put(22, 1);
      led_off = make_timeout_time_ms(100);
      if (tud_mounted()) {
        struct tile_packet pkt = {TILE_MAGIC, TILE_HEARTBEAT, 0};
        tud_vendor_write(&pkt, sizeof(pkt));
        tud_vendor_write_flush();
      }
    }

    if (time_reached(led_off)) {
      gpio_put(22, 0);
    }

    if (!tud_mounted()) {
      bytesRx = 0;
      continue;
    }

    if (bytesRx < sizeof(struct tile_packet)) {
      if (!tud_vendor_available()) continue;
      bytesRx +=
          tud_vendor_read(buf + bytesRx, sizeof(struct tile_packet) - bytesRx);
      if (bytesRx < sizeof(struct tile_packet)) {
        continue;
      }
    }

    struct tile_packet* pkt = (struct tile_packet*)buf;
    if (pkt->magic != TILE_MAGIC) {
      memmove(buf, buf + 1, sizeof(struct tile_packet) - 1);
      bytesRx = sizeof(struct tile_packet) - 1;
      continue;
    }

    switch (pkt->type) {}

    bytesRx = 0;
  }
}