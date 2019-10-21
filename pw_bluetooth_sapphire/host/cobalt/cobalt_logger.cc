// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/cobalt/cobalt_logger.h"

#include <fuchsia/cobalt/cpp/fidl.h>
#include <lib/async/cpp/task.h>

#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "src/connectivity/bluetooth/core/bt-host/cobalt/logger.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace cobalt {
namespace {

// Convert from the native Event type defined in bt::cobalt to the FIDL generated CobaltEvent type.
::fuchsia::cobalt::CobaltEvent BtEventToFidlEvent(Event event) {
  ::fuchsia::cobalt::CobaltEvent fidl_event{};
  fidl_event.metric_id = event.metric_id;
  fidl_event.event_codes = std::move(event.event_codes);

  class Visitor {
   public:
    Visitor(::fuchsia::cobalt::EventPayload* const payload) : payload_(payload) {}
    ::fuchsia::cobalt::EventPayload* const payload_;
    void operator()(EventCount count) {
      ::fuchsia::cobalt::CountEvent fidl_count{};
      fidl_count.count = count.value;
      payload_->set_event_count(fidl_count);
    }
    void operator()(ElapsedMicros elapsed_micros) {
      payload_->set_elapsed_micros(elapsed_micros.value);
    }
  };

  ::fuchsia::cobalt::EventPayload fidl_payload{};
  std::visit(Visitor{&fidl_payload}, event.payload);
  fidl_event.payload = std::move(fidl_payload);

  return fidl_event;
}

}  // namespace

fbl::RefPtr<CobaltLogger> CobaltLogger::Create() { return AdoptRef(new CobaltLogger()); }

CobaltLogger::CobaltLogger() : logger_(nullptr), dispatcher_(async_get_default_dispatcher()) {}

void CobaltLogger::Bind(::fidl::InterfaceHandle<::fuchsia::cobalt::Logger> logger) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  logger_.Bind(std::move(logger));
  logger_.set_error_handler(
      [](zx_status_t status) { bt_log(DEBUG, "bt-host", "CobaltLogger disconnected"); });
}

void CobaltLogger::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  if (logger_) {
    logger_.Unbind();
  }
}

void CobaltLogger::LogEvent(uint32_t metric_id, uint32_t event_code) {
  async::PostTask(dispatcher_, [objref = fbl::RefPtr(this), metric_id, event_code] {
    if (objref->logger_) {
      objref->logger_->LogEvent(metric_id, event_code, [](auto status) {});
    }
  });
}

void CobaltLogger::LogEventCount(uint32_t metric_id, uint32_t event_code, int64_t count) {
  async::PostTask(dispatcher_, [objref = fbl::RefPtr(this), metric_id, event_code, count] {
    if (objref->logger_) {
      objref->logger_->LogEventCount(metric_id, event_code, "", 0, count, [](auto status) {});
    }
  });
}

void CobaltLogger::LogElapsedTime(uint32_t metric_id, uint32_t event_code, int64_t elapsed_micros) {
  async::PostTask(dispatcher_, [objref = fbl::RefPtr(this), metric_id, event_code, elapsed_micros] {
    if (objref->logger_) {
      objref->logger_->LogElapsedTime(metric_id, event_code, "", elapsed_micros,
                                      [](auto status) {});
    }
  });
}

void CobaltLogger::LogCobaltEvent(Event event) {
  async::PostTask(dispatcher_, [objref = fbl::RefPtr(this), event = std::move(event)] {
    if (objref->logger_) {
      auto fidl_event = BtEventToFidlEvent(std::move(event));
      objref->logger_->LogCobaltEvent(std::move(fidl_event), [](auto status) {});
    }
  });
}

void CobaltLogger::LogCobaltEvents(std::vector<Event> events) {
  async::PostTask(dispatcher_, [objref = fbl::RefPtr(this), events = std::move(events)] {
    if (objref->logger_) {
      std::vector<::fuchsia::cobalt::CobaltEvent> fidl_events;
      fidl_events.resize(events.size());
      std::transform(events.begin(), events.end(), fidl_events.begin(), BtEventToFidlEvent);
      objref->logger_->LogCobaltEvents(std::move(fidl_events), [](auto status) {});
    }
  });
}

}  // namespace cobalt
}  // namespace bt
