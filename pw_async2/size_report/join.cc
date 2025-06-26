// Copyright 2025 The Pigweed Authors
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

#include "pw_async2/join.h"

#include "public/pw_async2/size_report/size_report.h"
#include "pw_assert/check.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/pendable.h"
#include "pw_async2/size_report/size_report.h"
#include "pw_bloat/bloat_this_binary.h"
#include "pw_log/log.h"

namespace pw::async2::size_report {

static Dispatcher dispatcher;

#ifdef _PW_ASYNC2_SIZE_REPORT_JOIN

int SingleTypeJoin(uint32_t mask) {
  PendableInt value_1(47);
  PendableInt value_2(52);
  PendableInt value_3(57);
  value_1.allow_completion = true;
  value_2.allow_completion = true;
  value_3.allow_completion = true;
  Join join(PendableFor<&PendableInt::Get>(value_1),
            PendableFor<&PendableInt::Get>(value_2),
            PendableFor<&PendableInt::Get>(value_3));
  auto result = dispatcher.RunPendableUntilStalled(join);
  PW_BLOAT_COND(result.IsReady(), mask);

  int value = -1;
  if (result.IsReady()) {
    value = std::get<0>(*result) + std::get<1>(*result) + std::get<2>(*result);
  }

  return value;
}

#ifdef _PW_ASYNC2_SIZE_REPORT_JOIN_INCREMENTAL

int MultiTypeJoin(uint32_t mask) {
  PendableInt value_1(47);
  PendableUint value_2(0x00ff00ff);
  PendableChar value_3('c');
  value_1.allow_completion = true;
  value_2.allow_completion = true;
  value_3.allow_completion = true;
  Join join(PendableFor<&PendableInt::Get>(value_1),
            PendableFor<&PendableUint::Get>(value_2),
            PendableFor<&PendableChar::Get>(value_3));
  auto result = dispatcher.RunPendableUntilStalled(join);
  PW_BLOAT_COND(result.IsReady(), mask);

  int value = -1;
  if (result.IsReady()) {
    value = std::get<0>(*result) + static_cast<int>(std::get<1>(*result)) +
            static_cast<int>(std::get<2>(*result));
  }
  return value;
}

#endif  // _PW_ASYNC2_SIZE_REPORT_JOIN_INCREMENTAL
#endif  // _PW_ASYNC2_SIZE_REPORT_JOIN

int Measure() {
  volatile uint32_t mask = bloat::kDefaultMask;
  SetBaseline(mask);

  MockTask task;
  dispatcher.Post(task);

  // Move the waker onto the stack to call its operator= before waking the task.
  Waker waker;
  PW_BLOAT_EXPR((waker = std::move(task.last_waker)), mask);
  std::move(waker).Wake();
  dispatcher.RunToCompletion();

  int result = -1;

#ifdef _PW_ASYNC2_SIZE_REPORT_JOIN
  result = SingleTypeJoin(mask);

#ifdef _PW_ASYNC2_SIZE_REPORT_JOIN_INCREMENTAL
  result += MultiTypeJoin(mask);
#endif  //_PW_ASYNC2_SIZE_REPORT_JOIN_INCREMENTAL

#endif  // _PW_ASYNC2_SIZE_REPORT_JOIN

  return result;
}

}  // namespace pw::async2::size_report

int main() { return pw::async2::size_report::Measure(); }
