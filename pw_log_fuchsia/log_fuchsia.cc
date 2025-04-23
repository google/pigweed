// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include <fidl/fuchsia.diagnostics.types/cpp/fidl.h>
#include <fidl/fuchsia.logger/cpp/fidl.h>
#include <lib/component/incoming/cpp/protocol.h>
#include <lib/syslog/structured_backend/cpp/fuchsia_syslog.h>
#include <zircon/process.h>

#include <cstdarg>
#include <cstdio>
#include <string_view>

#include "pw_assert/check.h"
#include "pw_log/levels.h"
#include "pw_log_fuchsia/log_backend.h"
#include "pw_string/string_builder.h"

namespace {

// This is an arbitrary size limit of logs.
constexpr size_t kBufferSize = 400;

// Returns the part of a path following the final '/', or the whole path if
// there is no '/'.
constexpr const char* BaseName(const char* path) {
  for (const char* c = path; c && (*c != '\0'); c++) {
    if (*c == '/') {
      path = c + 1;
    }
  }
  return path;
}

const char* LogLevelToString(int severity) {
  switch (severity) {
    case PW_LOG_LEVEL_ERROR:
      return "ERROR";
    case PW_LOG_LEVEL_WARN:
      return "WARN";
    case PW_LOG_LEVEL_INFO:
      return "INFO";
    case PW_LOG_LEVEL_DEBUG:
      return "DEBUG";
    default:
      return "UNKNOWN";
  }
}

FuchsiaLogSeverity FuchsiaLogSeverityFromFidl(
    fuchsia_diagnostics_types::Severity severity) {
  switch (severity) {
    case fuchsia_diagnostics_types::Severity::kFatal:
      return FUCHSIA_LOG_FATAL;
    case fuchsia_diagnostics_types::Severity::kError:
      return FUCHSIA_LOG_ERROR;
    case fuchsia_diagnostics_types::Severity::kWarn:
      return FUCHSIA_LOG_WARNING;
    case fuchsia_diagnostics_types::Severity::kInfo:
      return FUCHSIA_LOG_INFO;
    case fuchsia_diagnostics_types::Severity::kDebug:
      return FUCHSIA_LOG_DEBUG;
    case fuchsia_diagnostics_types::Severity::kTrace:
      return FUCHSIA_LOG_TRACE;
    default:
      return FUCHSIA_LOG_INFO;
  }
}

FuchsiaLogSeverity PigweedLevelToFuchsiaSeverity(int pw_level) {
  switch (pw_level) {
    case PW_LOG_LEVEL_ERROR:
      return FUCHSIA_LOG_ERROR;
    case PW_LOG_LEVEL_WARN:
      return FUCHSIA_LOG_WARNING;
    case PW_LOG_LEVEL_INFO:
      return FUCHSIA_LOG_INFO;
    case PW_LOG_LEVEL_DEBUG:
      return FUCHSIA_LOG_DEBUG;
    default:
      return FUCHSIA_LOG_ERROR;
  }
}

class LogState {
 public:
  void Initialize(async_dispatcher_t* dispatcher) {
    dispatcher_ = dispatcher;

    auto client_end = ::component::Connect<fuchsia_logger::LogSink>();
    PW_CHECK(client_end.is_ok());
    log_sink_.Bind(std::move(*client_end), dispatcher_);

    zx::socket local, remote;
    zx::socket::create(ZX_SOCKET_DATAGRAM, &local, &remote);
    ::fidl::OneWayStatus result =
        log_sink_->ConnectStructured(std::move(remote));
    PW_CHECK(result.ok());

    // Get interest level synchronously to avoid dropping DEBUG logs during
    // initialization (before an async interest response would be received).
    ::fidl::WireResult<::fuchsia_logger::LogSink::WaitForInterestChange>
        interest_result = log_sink_.sync()->WaitForInterestChange();
    PW_CHECK(interest_result.ok());
    HandleInterest(interest_result->value()->data);

    socket_ = std::move(local);

    WaitForInterestChanged();
  }

  void HandleInterest(fuchsia_diagnostics_types::wire::Interest& interest) {
    if (!interest.has_min_severity()) {
      severity_ = FUCHSIA_LOG_INFO;
    } else {
      severity_ = FuchsiaLogSeverityFromFidl(interest.min_severity());
    }
  }

  void WaitForInterestChanged() {
    log_sink_->WaitForInterestChange().Then(
        [this](fidl::WireUnownedResult<
               fuchsia_logger::LogSink::WaitForInterestChange>&
                   interest_result) {
          if (!interest_result.ok()) {
            auto error = interest_result.error();
            PW_CHECK(error.is_dispatcher_shutdown(),
                     "%s",
                     error.FormatDescription().c_str());
            return;
          }
          HandleInterest(interest_result.value()->data);
          WaitForInterestChanged();
        });
  }

  zx::socket& socket() { return socket_; }
  FuchsiaLogSeverity severity() const { return severity_; }

 private:
  fidl::WireClient<::fuchsia_logger::LogSink> log_sink_;
  async_dispatcher_t* dispatcher_;
  zx::socket socket_;
  FuchsiaLogSeverity severity_ = FUCHSIA_LOG_INFO;
};

LogState log_state;

zx_koid_t GetKoid(zx_handle_t handle) {
  zx_info_handle_basic_t info;
  zx_status_t status = zx_object_get_info(
      handle, ZX_INFO_HANDLE_BASIC, &info, sizeof(info), nullptr, nullptr);
  return status == ZX_OK ? info.koid : ZX_KOID_INVALID;
}

thread_local const zx_koid_t thread_koid = GetKoid(zx_thread_self());
zx_koid_t const process_koid = GetKoid(zx_process_self());

}  // namespace

namespace pw::log_fuchsia {

void InitializeLogging(async_dispatcher_t* dispatcher) {
  log_state.Initialize(dispatcher);
}

}  // namespace pw::log_fuchsia

extern "C" void pw_Log(int level,
                       const char* module_name,
                       unsigned int flags,
                       const char* file_name,
                       int line_number,
                       const char* message,
                       ...) {
  if (flags & PW_LOG_FLAG_IGNORE) {
    return;
  }

  pw::StringBuffer<kBufferSize> formatted;

  va_list args;
  va_start(args, message);
  formatted.FormatVaList(message, args);
  va_end(args);

  if (flags & PW_LOG_FLAG_USE_PRINTF) {
    printf("%s: [%s:%s:%d] %s\n",
           LogLevelToString(level),
           module_name,
           BaseName(file_name),
           line_number,
           formatted.c_str());
    return;
  }

  FuchsiaLogSeverity fuchsia_severity = PigweedLevelToFuchsiaSeverity(level);
  if (log_state.severity() > fuchsia_severity) {
    return;
  }

  ::fuchsia_syslog::LogBuffer buffer;
  buffer.BeginRecord(fuchsia_severity,
                     std::string_view(file_name),
                     line_number,
                     std::string_view(formatted.c_str()),
                     log_state.socket().borrow(),
                     /*dropped_count=*/0,
                     process_koid,
                     thread_koid);
  buffer.WriteKeyValue("tag", module_name);
  buffer.FlushRecord();
}
