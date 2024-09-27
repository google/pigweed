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

#include "pw_async2/dispatcher_base.h"
#include "pw_sync/thread_notification.h"

namespace pw::async2::backend {

// Windows GCC doesn't realize the nonvirtual destructor is protected and that
// the class is final.
PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wnon-virtual-dtor");

// Implementor's note:
//
// This class defines the "basic" backend for the ``Dispatcher`` facade.
//
// All public and private methods here are necessary when implementing a
// ``Dispatcher`` backend. The private methods are invoked via the
// ``DispatcherImpl` via CRTP.
//
// The ``Dispatcher`` type here is publicly exposed as
// ``pw::async2::Dispatcher``. Any additional backend-specific public methods
// should include a ``Native`` prefix to indicate that they are
// platform-specific extensions and are not portable to other ``pw::async2``
// backends.
class NativeDispatcher final : public NativeDispatcherBase {
 public:
  NativeDispatcher() = default;

  // NOTE: there are no public methods here because the public interface of
  // ``Dispatcher`` is defined in ``DispatcherImpl``.
  //
  // Any additional public methods added here (or in another ``Dispatcher``
  // backend) would be backend-specific and should be clearly marked with a
  // ``...Native`` suffix.
 private:
  friend class ::pw::async2::Dispatcher;

  void DoWake() final { notify_.release(); }
  Poll<> DoRunUntilStalled(Dispatcher&, Task* task);
  void DoRunToCompletion(Dispatcher&, Task* task);

  pw::sync::ThreadNotification notify_;
};

PW_MODIFY_DIAGNOSTICS_POP();

}  // namespace pw::async2::backend
