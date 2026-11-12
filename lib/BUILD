load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "tile_protocol",
    hdrs = ["tile_protocol.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "bounded_queue",
    hdrs = ["bounded_queue.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "connection",
    srcs = ["connection.cc"],
    hdrs = ["connection.h"],
    copts = ["-fno-exceptions"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "socket_server",
    srcs = ["socket_server.cc"],
    hdrs = ["socket_server.h"],
    copts = ["-fno-exceptions"],
    visibility = ["//visibility:public"],
    deps = [
        ":connection",
        "@abseil-cpp//absl/log",
    ],
)
