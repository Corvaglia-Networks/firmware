# Copyright (c) 2026 Corvaglia Networks
# SPDX-License-Identifier: MIT

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "imager",
    srcs = ["imager.cc"],
    hdrs = ["imager.h"],
    copts = ["-fno-exceptions"],
    deps = [
        "//lib:bounded_queue",
        "@abseil-cpp//absl/log",
    ],
)

cc_library(
    name = "tile_session",
    srcs = ["tile_session.cc"],
    hdrs = ["tile_session.h"],
    copts = ["-fno-exceptions"],
    deps = [
        "//lib:tile_protocol",
        "@abseil-cpp//absl/log",
        "@libusb",
    ],
)

cc_library(
    name = "supervisor",
    srcs = ["supervisor.cc"],
    hdrs = [
        "loopback_port.h",
        "port.h",
        "supervisor.h",
    ],
    copts = ["-fno-exceptions"],
    deps = [
        ":imager",
        ":tile_session",
        "//lib:socket_server",
        "//lib:tile_protocol",
        "@abseil-cpp//absl/log",
        "@libusb",
    ],
)

cc_binary(
    name = "hardwared",
    srcs = ["main.cc"],
    copts = ["-fno-exceptions"],
    linkopts = ["-pthread"],
    deps = [
        ":supervisor",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:globals",
        "@abseil-cpp//absl/log:initialize",
    ],
)
