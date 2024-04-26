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

#include "pw_function/function.h"

namespace pw::thread {

/// An old-style ThreadRoutine function pointer.
///
/// This is deprecated and should not be used in new code.
using DeprecatedFnPtr = void (*)(void* arg);

/// An old-style ThreadRoutine function pointer and argument.
///
/// This is deprecated and should not be used in new code.
struct DeprecatedFnPtrAndArg {
  DeprecatedFnPtr fn_ptr_;
  void* fn_arg_;
};

/// For internal thread backend implementation only.
///
/// This is used to store either a deprecated ThreadRoutine or a pw::Function.
class DeprecatedOrNewThreadFn {
 public:
  DeprecatedOrNewThreadFn() : fn_(), is_deprecated_(false) {}
  DeprecatedOrNewThreadFn(const DeprecatedOrNewThreadFn&) = delete;
  DeprecatedOrNewThreadFn& operator=(const DeprecatedOrNewThreadFn&) = delete;

  DeprecatedOrNewThreadFn(DeprecatedOrNewThreadFn&& other)
      : is_deprecated_(other.is_deprecated_) {
    if (other.is_deprecated_) {
      deprecated_ = other.deprecated_;
    } else {
      new (&fn_) Function<void()>(std::move(other.fn_));
    }
  }

  DeprecatedOrNewThreadFn& operator=(DeprecatedOrNewThreadFn&& other) {
    DestroyIfFunction();
    if (other.is_deprecated_) {
      deprecated_ = other.deprecated_;
      is_deprecated_ = true;
    } else {
      new (&fn_) Function<void()>(std::move(other.fn_));
      is_deprecated_ = false;
    }
    return *this;
  }

  DeprecatedOrNewThreadFn& operator=(std::nullptr_t) {
    *this = Function<void()>(nullptr);
    return *this;
  }

  DeprecatedOrNewThreadFn(DeprecatedFnPtrAndArg&& deprecated)
      : deprecated_(deprecated), is_deprecated_(true) {}

  DeprecatedOrNewThreadFn& operator=(DeprecatedFnPtrAndArg&& deprecated) {
    DestroyIfFunction();
    deprecated_ = deprecated;
    is_deprecated_ = true;
    return *this;
  }

  DeprecatedOrNewThreadFn(Function<void()>&& fn)
      : fn_(std::move(fn)), is_deprecated_(false) {}

  DeprecatedOrNewThreadFn& operator=(Function<void()>&& fn) {
    DestroyIfFunction();
    new (&fn_) Function<void()>(std::move(fn));
    is_deprecated_ = false;
    return *this;
  }

  ~DeprecatedOrNewThreadFn() { DestroyIfFunction(); }

  void operator()() {
    if (is_deprecated_) {
      deprecated_.fn_ptr_(deprecated_.fn_arg_);
    } else {
      fn_();
    }
  }

 private:
  void DestroyIfFunction() {
    if (!is_deprecated_) {
      std::destroy_at(&fn_);
    }
  }

  union {
    DeprecatedFnPtrAndArg deprecated_;
    Function<void()> fn_;
  };
  bool is_deprecated_;
};

}  // namespace pw::thread
