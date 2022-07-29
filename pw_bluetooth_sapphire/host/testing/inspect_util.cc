// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NINSPECT

#include "inspect_util.h"

#include <lib/fpromise/single_threaded_executor.h>

namespace bt::testing {

inspect::Hierarchy ReadInspect(const inspect::Inspector& inspector) {
  fpromise::single_threaded_executor executor;
  fpromise::result<inspect::Hierarchy> hierarchy;
  executor.schedule_task(inspect::ReadFromInspector(inspector).then(
      [&](fpromise::result<inspect::Hierarchy>& res) { hierarchy = std::move(res); }));
  executor.run();
  ZX_ASSERT(hierarchy.is_ok());
  return hierarchy.take_value();
}

}  // namespace bt::testing

#endif  // NINSPECT
