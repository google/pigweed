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

#include <cstring>

#include "pw_assert_trap/config.h"
#include "pw_assert_trap/handler.h"
#include "pw_assert_trap/message.h"
#include "pw_string/string_builder.h"
#include "pw_sync/interrupt_spin_lock.h"

static pw::sync::InterruptSpinLock interrupt_spin_lock;
PW_CONSTINIT static pw::InlineString<PW_ASSERT_TRAP_BUFFER_SIZE> message_buffer;

const std::string_view pw_assert_trap_get_message() { return message_buffer; }

void pw_assert_trap_clear_message() { message_buffer.clear(); }

void pw_assert_trap_interrupt_lock(void) { interrupt_spin_lock.lock(); }

void pw_assert_trap_interrupt_unlock(void) { interrupt_spin_lock.unlock(); }

void pw_assert_trap_HandleAssertFailure(
    [[maybe_unused]] const char* file_name,
    [[maybe_unused]] int line_number,
    [[maybe_unused]] const char* function_name) {
  pw::StringBuilder builder(message_buffer);

  builder.append("PW_ASSERT() or PW_DASSERT() failure");

#if !PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE
  builder.append(" at ");
  if (file_name != nullptr && line_number != -1) {
    builder.Format("%s:%d", file_name, line_number);
  }
  if (function_name != nullptr) {
    builder.Format(" %s: ", function_name);
  }
#endif  // !PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE
}

void pw_assert_trap_HandleCheckFailure(
    [[maybe_unused]] const char* file_name,
    [[maybe_unused]] int line_number,
    [[maybe_unused]] const char* function_name,
    const char* format,
    ...) {
  pw::StringBuilder builder(message_buffer);

#if !PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE
  if (file_name != nullptr && line_number != -1) {
    builder.Format("%s:%d", file_name, line_number);
  }
  if (function_name != nullptr) {
    builder.Format(" %s: ", function_name);
  }
#endif  // !PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE

  va_list args;
  va_start(args, format);
  builder.FormatVaList(format, args);
  va_end(args);
}
