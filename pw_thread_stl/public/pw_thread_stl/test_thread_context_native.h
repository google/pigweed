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

#include "pw_thread_stl/options.h"

namespace pw::thread::test::backend {

// Native Test Thread Options backend class for the C++ standard library.
class TestThreadContextNative {
 public:
  constexpr TestThreadContextNative() = default;

  TestThreadContextNative(const TestThreadContextNative&) = delete;
  TestThreadContextNative& operator=(const TestThreadContextNative&) = delete;

  ~TestThreadContextNative() = default;

  const Options& options() const { return options_; }

 private:
  stl::Options options_;
};

}  // namespace pw::thread::test::backend