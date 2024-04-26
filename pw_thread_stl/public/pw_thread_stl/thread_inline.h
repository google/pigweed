// Copyright 2020 The Pigweed Authors
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

#include <thread>

#include "pw_thread/thread.h"

namespace pw::thread {
namespace internal {

// When compiling with GCC and MinGW-w64 on Windows, std::thread::detach() can
// cause indefinite hangs due to issues with thread cleanup. This undefined
// symbol prevents binaries from linking if detach() is ever called on a thread
// in the final binary.
//
// It's not clear yet whether this goes away when using the official Windows
// SDK. For more information, see b/317922402.
#if defined(__MINGW32__) || defined(__MINGW64__)
[[noreturn]] void ErrorAttemptedToInvokeStdThreadDetachOnMinGW();
#endif  // defined(__MINGW32__) || defined(__MINGW64__)

}  // namespace internal

inline Thread::Thread() : native_type_() {}

inline Thread::Thread(const Options&, Function<void()>&& entry) {
  native_type_ = std::thread(std::move(entry));
}

inline Thread::Thread(const Options&, ThreadRoutine entry, void* arg) {
  native_type_ = std::thread(entry, arg);
}

inline Thread& Thread::operator=(Thread&& other) {
  native_type_ = std::move(other.native_type_);
  return *this;
}

inline Thread::~Thread() = default;

inline Id Thread::get_id() const { return native_type_.get_id(); }

inline void Thread::join() { native_type_.join(); }

inline void Thread::detach() {
#if defined(__MINGW32__) || defined(__MINGW64__)
  internal::ErrorAttemptedToInvokeStdThreadDetachOnMinGW();
#endif  // defined(__MINGW32__) || defined(__MINGW64__)
  native_type_.detach();
}

inline void Thread::swap(Thread& other) {
  native_type_.swap(other.native_handle());
}

inline Thread::native_handle_type Thread::native_handle() {
  return native_type_;
}

}  // namespace pw::thread
