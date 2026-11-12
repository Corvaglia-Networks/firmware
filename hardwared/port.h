// Copyright (c) 2026 Corvaglia Networks
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

class Port {
 public:
  virtual ~Port() = default;

  using Sink = std::function<void(const uint8_t* data, size_t size)>;

  virtual bool Write(const uint8_t* data, size_t size) = 0;

  virtual int Subscribe(Sink sink) = 0;
  virtual void Unsubscribe(int id) = 0;
};