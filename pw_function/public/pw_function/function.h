// Copyright 2021 The Pigweed Authors
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

#include "pw_function/internal/function.h"

namespace pw {

// pw::Function is a wrapper for an aribtrary callable object. It can be used by
// callback-based APIs to allow callers to provide any type of callable.
//
// Example:
//
//   template <typename T>
//   bool All(const pw::Vector<T>& items,
//            pw::Function<bool(const T& item)> predicate) {
//     for (const T& item : items) {
//       if (!predicate(item)) {
//         return false;
//       }
//     }
//     return true;
//   }
//
//   bool ElementsArePostive(const pw::Vector<int>& items) {
//     return All(items, [](const int& i) { return i > 0; });
//   }
//
//   bool IsEven(const int& i) { return i % 2 == 0; }
//
//   bool ElementsAreEven(const pw::Vector<int>& items) {
//     return All(items, IsEven);
//   }
//
template <typename Callable>
class Function {
  static_assert(std::is_function_v<Callable>,
                "pw::Function may only be instantianted for a function type, "
                "such as pw::Function<void(int)>.");
};

using Closure = Function<void()>;

template <typename Return, typename... Args>
class Function<Return(Args...)> {
 public:
  constexpr Function() = default;
  constexpr Function(std::nullptr_t) : Function() {}

  template <typename Callable>
  Function(Callable callable) {
    if (function_internal::IsNull(callable)) {
      holder_.InitializeNullTarget();
    } else {
      holder_.InitializeInlineTarget(std::move(callable));
    }
  }

  Function(Function&& other) {
    holder_.MoveInitializeTargetFrom(other.holder_);
    other.holder_.InitializeNullTarget();
  }

  Function& operator=(Function&& other) {
    holder_.DestructTarget();
    holder_.MoveInitializeTargetFrom(other.holder_);
    other.holder_.InitializeNullTarget();
    return *this;
  }

  Function& operator=(std::nullptr_t) {
    holder_.DestructTarget();
    holder_.InitializeNullTarget();
    return *this;
  }

  template <typename Callable>
  Function& operator=(Callable callable) {
    holder_.DestructTarget();
    if (function_internal::IsNull(callable)) {
      holder_.InitializeNullTarget();
    } else {
      holder_.InitializeInlineTarget(std::move(callable));
    }
    return *this;
  }

  ~Function() { holder_.DestructTarget(); }

  template <typename... PassedArgs>
  Return operator()(PassedArgs&&... args) const {
    return holder_.target()(std::forward<PassedArgs>(args)...);
  };

  explicit operator bool() const { return !holder_.target().IsNull(); }

 private:
  // TODO(frolv): This is temporarily private while the API is worked out.
  template <typename Callable, size_t kSizeBytes>
  Function(Callable&& callable,
           function_internal::FunctionStorage<kSizeBytes>& storage)
      : Function(callable, &storage) {
    static_assert(sizeof(Callable) <= kSizeBytes,
                  "pw::Function callable does not fit into provided storage");
  }

  // Constructs a function that stores its callable at the provided location.
  // Public constructors wrapping this must ensure that the memory region is
  // capable of storing the callable in terms of both size and alignment.
  template <typename Callable>
  Function(Callable&& callable, void* storage) {
    if (function_internal::IsNull(callable)) {
      holder_.InitializeNullTarget();
    } else {
      holder_.InitializeMemoryTarget(std::forward(callable), storage);
    }
  }

  function_internal::FunctionTargetHolder<
      function_internal::config::kInlineCallableSize,
      Return,
      Args...>
      holder_;
};

// nullptr comparisions for functions.
template <typename T>
bool operator==(const Function<T>& f, std::nullptr_t) {
  return !static_cast<bool>(f);
}

template <typename T>
bool operator!=(const Function<T>& f, std::nullptr_t) {
  return static_cast<bool>(f);
}

template <typename T>
bool operator==(std::nullptr_t, const Function<T>& f) {
  return !static_cast<bool>(f);
}

template <typename T>
bool operator!=(std::nullptr_t, const Function<T>& f) {
  return static_cast<bool>(f);
}

}  // namespace pw
