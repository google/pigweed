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

#include "chre/platform/atomic.h"

namespace chre {

inline AtomicBool::AtomicBool(bool starting_value) {
  std::atomic_init(&atomic_, starting_value);
}

inline bool AtomicBool::operator=(bool desired) { return atomic_ = desired; }

inline bool AtomicBool::load() const { return atomic_.load(); }

inline void AtomicBool::store(bool desired) { atomic_.store(desired); }

inline bool AtomicBool::exchange(bool desired) {
  return atomic_.exchange(desired);
}

inline AtomicUint32::AtomicUint32(uint32_t startingValue) {
  std::atomic_init(&atomic_, startingValue);
}

inline uint32_t AtomicUint32::operator=(uint32_t desired) {
  return atomic_ = desired;
}

inline uint32_t AtomicUint32::load() const { return atomic_.load(); }

inline void AtomicUint32::store(uint32_t desired) { atomic_.store(desired); }

inline uint32_t AtomicUint32::exchange(uint32_t desired) {
  return atomic_.exchange(desired);
}

inline uint32_t AtomicUint32::fetch_add(uint32_t arg) {
  return atomic_.fetch_add(arg);
}

inline uint32_t AtomicUint32::fetch_increment() { return atomic_.fetch_add(1); }

inline uint32_t AtomicUint32::fetch_sub(uint32_t arg) {
  return atomic_.fetch_sub(arg);
}

inline uint32_t AtomicUint32::fetch_decrement() { return atomic_.fetch_sub(1); }

}  // namespace chre
