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

/// @file pw_function/pointer.h
///
/// Traditional callback APIs often use a function pointer and `void*` context
/// argument. The context argument makes it possible to use the callback
/// function with non-global data. For example, the `qsort_s` and `bsearch_s`
/// functions take a pointer to a comparison function that has `void*` context
/// as its last parameter. @cpp_type{pw::Function} does not naturally work with
/// these kinds of APIs.
///
/// The functions below make it simple to adapt a @cpp_type{pw::Function} for
/// use with APIs that accept a function pointer and `void*` context argument.

#include <utility>

#include "pw_function/internal/static_invoker.h"

namespace pw::function {

/// Returns a function pointer that invokes a `pw::Function`, lambda, or other
/// callable object from a `void*` context argument. This makes it possible to
/// use C++ callables with C-style APIs that take a function pointer and `void*`
/// context.
///
/// The returned function pointer has the same return type and arguments as the
/// `pw::Function` or `pw::Callback`, except that the last parameter is a
/// `void*`. `GetFunctionPointerContextFirst` places the `void*` context
/// parameter first.
///
/// The following example adapts a C++ lambda function for use with C-style API
/// that takes an `int (*)(int, void*)` function and a `void*` context.
///
/// @code{.cpp}
///
///   void TakesAFunctionPointer(int (*function)(int, void*), void* context);
///
///   void UseFunctionPointerApiWithPwFunction() {
///     // Declare a callable object so a void* pointer can be obtained for it.
///     auto my_function = [captures](int value) {
///        // ...
///        return value + captures;
///     };
///
///     // Invoke the API with the function pointer and callable pointer.
///     TakesAFunctionPointer(pw::function::GetFunctionPointer(my_function),
///                           &my_function);
///   }
///
/// @endcode
///
/// The function returned from this must ONLY be used with the exact type for
/// which it was created! Function pointer / context APIs are not type safe.
template <typename FunctionType>
constexpr auto GetFunctionPointer() {
  return internal::StaticInvoker<
      FunctionType,
      decltype(&FunctionType::operator())>::InvokeWithContextLast;
}

/// `GetFunctionPointer` overload that uses the type of the function passed to
/// this call.
template <typename FunctionType>
constexpr auto GetFunctionPointer(const FunctionType&) {
  return GetFunctionPointer<FunctionType>();
}

/// Same as `GetFunctionPointer`, but the context argument is passed first.
/// Returns a `void(void*, int)` function for a `pw::Function<void(int)>`.
template <typename FunctionType>
constexpr auto GetFunctionPointerContextFirst() {
  return internal::StaticInvoker<
      FunctionType,
      decltype(&FunctionType::operator())>::InvokeWithContextFirst;
}

/// `GetFunctionPointerContextFirst` overload that uses the type of the function
/// passed to this call.
template <typename FunctionType>
constexpr auto GetFunctionPointerContextFirst(const FunctionType&) {
  return GetFunctionPointerContextFirst<FunctionType>();
}

}  // namespace pw::function
