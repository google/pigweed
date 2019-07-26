// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection_parameters.h"

namespace bt {
namespace hci {

namespace {

// The length of a timeslice in the parameters, in milliseconds.
constexpr static float kTimesliceMs = 1.25f;

}  // namespace

LEConnectionParameters::LEConnectionParameters(uint16_t interval, uint16_t latency,
                                               uint16_t supervision_timeout)
    : interval_(interval), latency_(latency), supervision_timeout_(supervision_timeout) {}

LEConnectionParameters::LEConnectionParameters()
    : interval_(0), latency_(0), supervision_timeout_(0) {}

bool LEConnectionParameters::operator==(const LEConnectionParameters& other) const {
  return interval_ == other.interval_ && latency_ == other.latency_ &&
         supervision_timeout_ == other.supervision_timeout_;
}

std::string LEConnectionParameters::ToString() const {
  return fxl::StringPrintf("interval: %.2f ms, latency: %.2f ms, timeout: %u ms",
                           static_cast<float>(interval_) * kTimesliceMs,
                           static_cast<float>(latency_) * kTimesliceMs, supervision_timeout_ * 10u);
}

LEPreferredConnectionParameters::LEPreferredConnectionParameters(uint16_t min_interval,
                                                                 uint16_t max_interval,
                                                                 uint16_t max_latency,
                                                                 uint16_t supervision_timeout)
    : min_interval_(min_interval),
      max_interval_(max_interval),
      max_latency_(max_latency),
      supervision_timeout_(supervision_timeout) {}

LEPreferredConnectionParameters::LEPreferredConnectionParameters()
    : min_interval_(0), max_interval_(0), max_latency_(0), supervision_timeout_(0) {}

bool LEPreferredConnectionParameters::operator==(
    const LEPreferredConnectionParameters& other) const {
  return min_interval_ == other.min_interval_ && max_interval_ == other.max_interval_ &&
         max_latency_ == other.max_latency_ && supervision_timeout_ == other.supervision_timeout_;
}

}  // namespace hci
}  // namespace bt
