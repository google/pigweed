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
#pragma once

namespace pw::chrono {

/// An abstract interface representing a Clock.
///
/// This interface allows decoupling code that uses time from the code that
/// creates a point in time. You can use this to your advantage by injecting
/// Clocks into interfaces rather than having implementations call
/// `SystemClock::now()` directly. However, this comes at a cost of a vtable per
/// implementation and more importantly passing and maintaining references to
/// the VirtualClock for all of the users.
///
/// This interface is thread and IRQ safe.
template <typename Clock>
class VirtualClock {
 public:
  virtual ~VirtualClock() = default;

  /// Returns the current time.
  virtual typename Clock::time_point now() = 0;
};

}  // namespace pw::chrono
