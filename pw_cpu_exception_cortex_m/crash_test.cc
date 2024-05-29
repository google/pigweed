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
#include "pw_cpu_exception_cortex_m/crash.h"

#include <cstdarg>
#include <mutex>

#include "pw_cpu_exception_cortex_m/cpu_state.h"
#include "pw_cpu_exception_cortex_m_private/cortex_m_constants.h"
#include "pw_status/status.h"
#include "pw_string/string_builder.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_unit_test/framework.h"

namespace pw::cpu_exception::cortex_m {
namespace {
const char* kSampleThreadName = "BadThread";

class FakeCrashHandler : public testing::Test {
 public:
  void CaptureCrashAnalysis(const char* fmt, va_list args) {
    StringBuilder message_builder(buffer_);
    ASSERT_EQ(message_builder.FormatVaList(fmt, args).status(), OkStatus());
  }

 protected:
  std::array<char, 124> buffer_;
};

pw::sync::Mutex crash_handler_lock;
FakeCrashHandler* crash_handler PW_GUARDED_BY(crash_handler_lock) = nullptr;

TEST_F(FakeCrashHandler, CaptureCrashInfoDivideByZero) {
  std::lock_guard lock(crash_handler_lock);
  crash_handler = this;
  pw_cpu_exception_State cpu_state = {};
  cpu_state.extended.cfsr = kCfsrDivbyzeroMask;
  AnalyzeCpuStateAndCrash(cpu_state, kSampleThreadName);
  std::string_view analysis{buffer_.data(), sizeof(buffer_)};
  EXPECT_NE(analysis.find(kSampleThreadName), static_cast<size_t>(0));
  EXPECT_NE(analysis.find("DIVBYZERO"), static_cast<size_t>(0));
  crash_handler = nullptr;
}

TEST_F(FakeCrashHandler, CaptureCrashInfoNoThreadName) {
  std::lock_guard lock(crash_handler_lock);
  crash_handler = this;
  pw_cpu_exception_State cpu_state = {};
  cpu_state.extended.cfsr = kCfsrDivbyzeroMask;
  AnalyzeCpuStateAndCrash(cpu_state, nullptr);
  std::string_view analysis{buffer_.data(), sizeof(buffer_)};
  EXPECT_NE(analysis.find(kSampleThreadName), static_cast<size_t>(0));
  EXPECT_NE(analysis.find("Thread=?"), static_cast<size_t>(0));
  crash_handler = nullptr;
}

}  // namespace

void CaptureCrashAnalysisForTest(const pw_cpu_exception_State&,
                                 const char* fmt,
                                 ...) {
  ASSERT_NE(crash_handler, nullptr);
  va_list args;
  va_start(args, fmt);
  crash_handler->CaptureCrashAnalysis(fmt, args);
  va_end(args);
}

}  // namespace pw::cpu_exception::cortex_m
