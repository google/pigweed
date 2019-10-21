// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COBALT_COBALT_LOGGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COBALT_COBALT_LOGGER_H_

#include <fuchsia/cobalt/cpp/fidl.h>
#include <lib/async/default.h>

#include <mutex>

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "src/connectivity/bluetooth/core/bt-host/cobalt/logger.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace cobalt {

// CobaltLogger allows events to be sent to the Fuchsia cobalt service. A CobaltLogger that is not
// bound to an open cobalt::Logger channel simply ignores all logged events.
class CobaltLogger final : public Logger {
 public:
  static fbl::RefPtr<CobaltLogger> Create();
  // CobaltLogger::Set is not thread-safe and must always be called from the constructor thread.
  void Bind(::fidl::InterfaceHandle<::fuchsia::cobalt::Logger> logger);
  // CobaltLogger::ShutDown must be called to unbind the handle to the Cobalt service.
  // It is not thread-safe and must always be called from the Constructor thread.
  void ShutDown();

  // Logger overrides:
  void LogEvent(uint32_t metric_id, uint32_t event_code) override;
  void LogEventCount(uint32_t metric_id, uint32_t event_code, int64_t count) override;
  void LogElapsedTime(uint32_t metric_id, uint32_t event_code, int64_t elapsed_micros) override;
  void LogCobaltEvent(Event event) override;
  void LogCobaltEvents(std::vector<Event> events) override;

 private:
  friend class fbl::RefPtr<CobaltLogger>;
  explicit CobaltLogger();
  ~CobaltLogger() override = default;
  ::fuchsia::cobalt::LoggerPtr logger_;

  fxl::ThreadChecker thread_checker_;
  async_dispatcher_t* dispatcher_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(CobaltLogger);
};

}  // namespace cobalt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COBALT_COBALT_LOGGER_H_
