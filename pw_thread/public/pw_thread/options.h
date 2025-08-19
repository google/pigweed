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

namespace pw::thread {

/// @module{pw_thread}

/// The Options contains the parameters needed for a thread to start.
///
/// Options are backend specific and ergo the generic base class cannot be
/// directly instantiated.
///
/// The attributes which can be set through the options are backend specific
/// but may contain things like the thread name, priority, scheduling policy,
/// core/processor affinity, and/or an optional reference to a pre-allocated
/// Context (the collection of memory allocations needed for a thread to run).
///
/// Options shall NOT have an attribute to start threads as detached vs
/// joinable. All `pw::Thread` instances must be explicitly `join()`'d or
/// `detach()`'d through the run-time Thread API.
///
/// Note that if backends set `PW_THREAD_JOINING_ENABLED` to false, backends may
/// use native OS specific APIs to create native detached threads because the
/// `join()` API would be compiled out. However, users must still explicitly
/// invoke `detach()`.
///
/// Options must not contain any memory needed for a thread to run (TCB,
/// stack, etc.). The Options may be deleted or re-used immediately after
/// starting a thread.
class Options {
 protected:
  // Must be explicit to prevent direct instantiation in C++17 with
  // `pw::thread::Options{}` syntax.
  explicit constexpr Options() = default;
};

}  // namespace pw::thread
