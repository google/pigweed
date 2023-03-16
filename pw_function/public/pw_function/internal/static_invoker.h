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

// This code is in its own header since Doxygen / Breathe can't parse it.

namespace pw::function::internal {

template <typename FunctionType, typename CallOperatorType>
struct StaticInvoker;

template <typename FunctionType, typename Return, typename... Args>
struct StaticInvoker<FunctionType, Return (FunctionType::*)(Args...)> {
  // Invoker function with the context argument last to match libc. Could add a
  // version with the context first if needed.
  static Return InvokeWithContextLast(Args... args, void* context) {
    return static_cast<FunctionType*>(context)->operator()(
        std::forward<Args>(args)...);
  }

  static Return InvokeWithContextFirst(void* context, Args... args) {
    return static_cast<FunctionType*>(context)->operator()(
        std::forward<Args>(args)...);
  }
};

// Make the const version identical to the non-const version.
template <typename FunctionType, typename Return, typename... Args>
struct StaticInvoker<FunctionType, Return (FunctionType::*)(Args...) const>
    : StaticInvoker<FunctionType, Return (FunctionType::*)(Args...)> {};

}  // namespace pw::function::internal
