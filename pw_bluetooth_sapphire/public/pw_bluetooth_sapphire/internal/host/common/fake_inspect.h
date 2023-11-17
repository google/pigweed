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

// This file contains a minimal fake implementation of Inspect to enable code to
// compile when Inspect is not supported.

#include <string>

namespace inspect {

template <typename T>
class Property {
 public:
  explicit operator bool() const { return false; }
  void Set(const T& value) {}
  void Add(T value) {}
  void Subtract(T value) {}
};

class StringProperty final : public Property<std::string> {};
class BoolProperty final : public Property<bool> {};
class IntProperty final : public Property<int64_t> {};
class UintProperty final : public Property<uint64_t> {};
class DoubleProperty final : public Property<double> {};

class Node final {
 public:
  Node() = default;

  explicit operator bool() const { return false; }

  Node CreateChild(std::string_view name) { return Node(); }

  std::string UniqueName(const std::string& prefix) { return ""; }

  StringProperty CreateString(std::string_view name, const std::string& value) {
    return {};
  }

  template <typename T>
  void CreateString(std::string_view name, const std::string& value, T* list) {}

  void RecordString(std::string_view name, const std::string& value) {}

  BoolProperty CreateBool(std::string_view name, bool value) { return {}; }

  void RecordBool(std::string_view name, bool value) {}

  IntProperty CreateInt(std::string_view, int64_t value) { return {}; }

  void RecordInt(std::string_view name, int64_t value) {}

  UintProperty CreateUint(std::string_view, uint64_t value) { return {}; }

  void RecordUint(std::string_view name, uint64_t value) {}

  DoubleProperty CreateDouble(std::string_view, double value) { return {}; }

  void RecordDouble(std::string_view name, double value) {}
};

class Inspector final {
 public:
  Node& GetRoot() { return node_; }

 private:
  Node node_;
};

}  // namespace inspect
