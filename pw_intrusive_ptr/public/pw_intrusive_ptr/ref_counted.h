// Copyright 2025 The Pigweed Authors
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

#include "pw_intrusive_ptr/internal/ref_counted_base.h"
#include "pw_intrusive_ptr/intrusive_ptr.h"

namespace pw {

// Base class to be used with the IntrusivePtr. Doesn't provide any public
// methods.
//
// Provides an atomic-based reference counting. Atomics are used irrespective of
// the settings, which makes it different from the std::shared_ptr (that relies
// on the threading support settings to determine if atomics should be used for
// the control block or not).
//
// RefCounted MUST never be used as a pointer type to store derived objects -
// it doesn't provide a virtual destructor.
template <typename T>
class RefCounted : private internal::RefCountedBase {
 public:
  // Type alias for the IntrusivePtr of ref-counted type.
  using Ptr = IntrusivePtr<T>;

 private:
  template <typename U>
  friend class IntrusivePtr;
};

}  // namespace pw
