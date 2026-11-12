# Copyright (c) 2026 Corvaglia Networks
# SPDX-License-Identifier: MIT

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "tusb_config",
    hdrs = ["tusb_config.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "tile.elf",
    srcs = [
        "main.c",
        "usb_descriptors.c",
    ],
    deps = [
        "//lib:tile_protocol",
        "@pico-sdk//bazel/config:PICO_TINYUSB_LIB",
        "@pico-sdk//src/rp2_common/pico_stdlib",
        "@pico-sdk//src/rp2_common/pico_unique_id",
    ],
)
