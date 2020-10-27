// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COBALT_LOGGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COBALT_LOGGER_H_

#include <lib/async/dispatcher.h>

#include <variant>
#include <vector>

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::cobalt {

// Used to log that an event has occurred a given number of times. Using this
// struct with LogCobaltEvent() is equivalent to invoking LogEventCount().
struct EventCount {
  // The number of times the event occurred
  uint64_t value{};
};

// This maps to a call to LogElapsedTime().
struct ElapsedMicros {
  uint64_t value{};
};

// The variadic part of an Event.
using EventPayload = std::variant<EventCount, ElapsedMicros>;

// A specification of an event that occurred to be passed to LogCobaltEvent().
// This API can be used to allow multiple event codes.
struct Event {
 public:
  Event(uint32_t metric_id, std::vector<uint32_t> event_codes, EventPayload payload)
      : metric_id(metric_id), event_codes(std::move(event_codes)), payload(payload){};
  // ID of the metric to use. It must be one of the metrics from the
  // project profile used to obtain this Logger, and its type must match the
  // `payload` type.
  uint32_t metric_id;
  // The event codes for the event that occurred. There must be one event code
  // given for each dimension specified in the metric definition.
  std::vector<uint32_t> event_codes;
  // The event-specific information for the event to be logged.
  EventPayload payload;
};

// Interface for metrics logging implementations. This interface roughly matches the
// fuchsia::cobalt::Logger protocol. The interface does not expose any FIDL specifics and can be
// used without directly depending on FIDL.
class Logger : public fbl::RefCounted<Logger> {
 public:
  // See https://fuchsia.dev/reference/fidl/fuchsia.cobalt#Logger for information on these method
  // calls
  virtual void LogEvent(uint32_t metric_id, uint32_t event_code) = 0;
  virtual void LogEventCount(uint32_t metric_id, uint32_t event_code, int64_t count) = 0;
  virtual void LogElapsedTime(uint32_t metric_id, uint32_t event_code, int64_t elapsed_micros) = 0;
  virtual void LogCobaltEvent(Event event) = 0;
  virtual void LogCobaltEvents(std::vector<Event> events) = 0;

 protected:
  friend class fbl::RefPtr<Logger>;
  Logger() = default;
  virtual ~Logger() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Logger);
};

}  // namespace bt::cobalt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COBALT_LOGGER_H_
