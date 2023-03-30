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

#include "pw_thread_backend/test_thread_context_native.h"

namespace pw::thread::test {

/// The class `TestThreadContext` is a facade class for thread testing across
/// platforms. The usage is similar to using `TestOptionsThread0()`, but user
/// doesn't need to hard-code backend dependency in the test. User needs to
/// implement the `pw::thread::test::backend::TestThreadContextNative` class in
/// `test_thread_context.h` and set the facade backend with
/// `pw_thread_TEST_THREAD_CONTEXT_BACKEND`.
/// User needs to ensure the context's lifespan outlives the thread it
/// created. Recyling or destroy context is only allowed for using `join()` in
/// thread.
/// WARNING: Threads using the `TestThreadContext` may only be detached if the
/// context has a static lifetime, meaning the context is both never re-used and
/// not destroyed before the end of the lifetime of the application.
///
/// @code{.cpp}
///    pw::thread::test::TestThreadContext context;
///    pw::thread::Thread test_thread(context.options(),
///                                   ExampleThreadFunction);
/// @endcode
///
/// IMPORTANT: Developers typically test thread logic inside a thread, which
/// should not need a thread to be spawned in most cases. It is recommended to
/// isolate thread logic in a separate function so that such function can be
/// called outside the thread scope in tests. Unit tests should avoid spawning
/// threads unless absolutely necessary.
class TestThreadContext {
 public:
  TestThreadContext() = default;

  TestThreadContext(const TestThreadContext&) = delete;
  TestThreadContext& operator=(const TestThreadContext&) = delete;

  /// @brief `pw::thread::test::TestThreadContext` returns a default
  /// `pw::thread::Options` associated with the current object for testing.
  ///
  /// @return The default options for testing thread.
  const Options& options() const { return context_.options(); }

 private:
  backend::TestThreadContextNative context_;
};

}  // namespace pw::thread::test
