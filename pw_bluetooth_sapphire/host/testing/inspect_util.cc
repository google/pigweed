// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/testing/inspect_util.h"

#ifndef NINSPECT

#include <lib/fpromise/single_threaded_executor.h>

namespace bt::testing {

inspect::Hierarchy ReadInspect(const inspect::Inspector& inspector) {
  fpromise::single_threaded_executor executor;
  fpromise::result<inspect::Hierarchy> hierarchy;
  executor.schedule_task(inspect::ReadFromInspector(inspector).then(
      [&](fpromise::result<inspect::Hierarchy>& res) {
        hierarchy = std::move(res);
      }));
  executor.run();
  BT_ASSERT(hierarchy.is_ok());
  return hierarchy.take_value();
}

}  // namespace bt::testing

#endif  // NINSPECT
