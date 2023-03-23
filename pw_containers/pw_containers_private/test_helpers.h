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
  explicit MoveOnly(int val) : value(val) {}

  MoveOnly(const MoveOnly&) = delete;

  MoveOnly(MoveOnly&& other) {
    value = other.value;
    other.value = kDeleted;
  }

  static constexpr int kDeleted = -1138;

  int value;
};

struct Counter {
  static int created;
  static int destroyed;
  static int moved;

  static void Reset() { created = destroyed = moved = 0; }

  Counter() : value(0) { created += 1; }

  Counter(int val) : value(val) { created += 1; }

  Counter(const Counter& other) : value(other.value) { created += 1; }

  Counter(Counter&& other) : value(other.value) {
    other.value = 0;
    moved += 1;
  }

  Counter& operator=(const Counter& other) {
    value = other.value;
    created += 1;
    return *this;
  }

  Counter& operator=(Counter&& other) {
    value = other.value;
    other.value = 0;
    moved += 1;
    return *this;
  }

  ~Counter() { destroyed += 1; }

  int value;
};

}  // namespace pw::containers::test
