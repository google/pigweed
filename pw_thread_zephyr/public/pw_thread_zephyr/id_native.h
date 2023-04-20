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
#pragma once

#include <zephyr/kernel.h>

namespace pw::thread::backend {

// Trivial wrapper around Zephyr RTOS-specific k_tid_t type
// (note that k_tid_t is just a pointer to the k_thread aka TCB).
class NativeId {
 public:
  constexpr NativeId(const k_tid_t thread_id = nullptr)
      : thread_id_(thread_id) {}

  constexpr bool operator==(NativeId other) const {
    return thread_id_ == other.thread_id_;
  }
  constexpr bool operator!=(NativeId other) const {
    return thread_id_ != other.thread_id_;
  }
  constexpr bool operator<(NativeId other) const {
    return thread_id_ < other.thread_id_;
  }
  constexpr bool operator<=(NativeId other) const {
    return thread_id_ <= other.thread_id_;
  }
  constexpr bool operator>(NativeId other) const {
    return thread_id_ > other.thread_id_;
  }
  constexpr bool operator>=(NativeId other) const {
    return thread_id_ >= other.thread_id_;
  }

 private:
  const k_tid_t thread_id_;
};

}  // namespace pw::thread::backend
