/*
 * Derived from TinyUSB examples.
 * The MIT License (MIT)
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 */

#include "lib/tile_protocol.h"
#include "pico/unique_id.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = TILE_USB_VID,
    .idProduct = TILE_USB_PID,
    .bcdDevice = 0x0001,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

uint8_t const* tud_descriptor_device_cb(void) {
  return (uint8_t const*)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum { ITF_NUM_VENDOR, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

#define EPNUM_VENDOR_OUT 0x01
#define EPNUM_VENDOR_IN 0x81

static uint8_t const desc_configuration[] = {
    // config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // interface number, string index, EP out, EP in, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 0, EPNUM_VENDOR_OUT, EPNUM_VENDOR_IN,
                          CFG_TUD_VENDOR_EPSIZE)};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_COUNT,
};

static char const* const string_desc_arr[] = {
    [STRID_MANUFACTURER] = "Corvaglia Networks",
    [STRID_PRODUCT] = "picoconsole",
};

#define DESC_STR_MAX 32
static uint16_t _desc_str[DESC_STR_MAX + 1];

static size_t ascii_to_utf16(uint16_t* dst, char const* src) {
  size_t n = strlen(src);
  if (n > DESC_STR_MAX) n = DESC_STR_MAX;
  for (size_t i = 0; i < n; i++) dst[i] = (uint8_t)src[i];
  return n;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  size_t chr_count;

  switch (index) {
    case STRID_LANGID:
      _desc_str[1] = 0x0409;
      chr_count = 1;
      break;

    case STRID_SERIAL: {
      char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
      pico_get_unique_board_id_string(serial, sizeof serial);
      chr_count = ascii_to_utf16(_desc_str + 1, serial);
      break;
    }

    case STRID_MANUFACTURER:
    case STRID_PRODUCT:
      chr_count = ascii_to_utf16(_desc_str + 1, string_desc_arr[index]);
      break;

    default:
      return NULL;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
