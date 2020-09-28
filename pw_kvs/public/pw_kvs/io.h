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

#include <cstddef>
#include <span>
#include <type_traits>

#include "pw_status/status_with_size.h"

namespace pw {
namespace internal {

template <typename T>
struct FunctionTraits;

template <typename T, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (T::*)(Args...)> {
  using Class = T;
  using Return = ReturnType;
};

}  // namespace internal

// Writes bytes to an unspecified output. Provides a Write function that takes a
// std::span of bytes and returns a Status.
class Output {
 public:
  StatusWithSize Write(std::span<const std::byte> data) {
    return DoWrite(data);
  }

  // Convenience wrapper for writing data from a pointer and length.
  StatusWithSize Write(const void* data, size_t size_bytes) {
    return Write(std::span(static_cast<const std::byte*>(data), size_bytes));
  }

 protected:
  ~Output() = default;

 private:
  virtual StatusWithSize DoWrite(std::span<const std::byte> data) = 0;
};

class Input {
 public:
  StatusWithSize Read(std::span<std::byte> data) { return DoRead(data); }

  // Convenience wrapper for reading data from a pointer and length.
  StatusWithSize Read(void* data, size_t size_bytes) {
    return Read(std::span(static_cast<std::byte*>(data), size_bytes));
  }

 protected:
  ~Input() = default;

 private:
  virtual StatusWithSize DoRead(std::span<std::byte> data) = 0;
};

// Output adapter that calls a method on a class with a std::span of bytes. If
// the method returns void instead of the expected Status, Write always returns
// Status::Ok().
template <auto kMethod>
class OutputToMethod final : public Output {
  using Class = typename internal::FunctionTraits<decltype(kMethod)>::Class;

 public:
  constexpr OutputToMethod(Class* object) : object_(*object) {}

 private:
  StatusWithSize DoWrite(std::span<const std::byte> data) override {
    using Return = typename internal::FunctionTraits<decltype(kMethod)>::Return;

    if constexpr (std::is_void_v<Return>) {
      (object_.*kMethod)(data);
      return StatusWithSize(data.size());
    } else {
      return (object_.*kMethod)(data);
    }
  }

 private:
  Class& object_;
};

// Output adapter that calls a free function.
class OutputToFunction final : public Output {
 public:
  OutputToFunction(StatusWithSize (*function)(std::span<const std::byte>))
      : function_(function) {}

 private:
  StatusWithSize DoWrite(std::span<const std::byte> data) override {
    return function_(data);
  }

  StatusWithSize (*function_)(std::span<const std::byte>);
};

}  // namespace pw
