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

#include <cstddef>

namespace pw::containers::test {

struct CopyOnly {
  explicit CopyOnly(int val) : value(val) {}

  CopyOnly(const CopyOnly& other) { value = other.value; }

  CopyOnly& operator=(const CopyOnly& other) {
    value = other.value;
    return *this;
  }

  CopyOnly(CopyOnly&&) = delete;

  int value;
};

struct MoveOnly {
  explicit constexpr MoveOnly(int val) : value(val) {}

  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  constexpr MoveOnly(MoveOnly&& other) : value(other.value) {
    other.value = kDeleted;
  }
  constexpr MoveOnly& operator=(MoveOnly&& other) {
    value = other.value;
    other.value = kDeleted;
    return *this;
  }

  static constexpr int kDeleted = -1138;

  int value;
};

// Counter objects CANNOT be globally scoped.
struct Counter {
  static int created;
  static int destroyed;
  static int moved;

  static void Reset() { created = destroyed = moved = 0; }

  Counter(int val = 0) : value(val), set_to_this_when_constructed_(this) {
    objects_.Constructed();
    created += 1;
  }

  Counter(const Counter& other) : Counter(other.value) {}

  Counter(Counter&& other)
      : value(other.value), set_to_this_when_constructed_(this) {
    objects_.Constructed();
    other.value = 0;
    moved += 1;
  }

  Counter& operator=(const Counter& other);
  Counter& operator=(Counter&& other);

  // Convert to int for easy comparisons.
  operator int() const { return value; }

  ~Counter();

  int value;

 private:
  class ObjectCounter {
   public:
    constexpr ObjectCounter() : count_(0) {}

    ~ObjectCounter();

    void Constructed() { count_ += 1; }
    void Destructed();

   private:
    size_t count_;
  };

  static ObjectCounter objects_;

  Counter* set_to_this_when_constructed_;
};

}  // namespace pw::containers::test
