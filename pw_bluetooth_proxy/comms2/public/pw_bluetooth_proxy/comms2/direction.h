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

#include <stddef.h>

namespace pw::bluetooth::proxy {

// Indicates direction a packet is traveling.
enum class Direction : bool {
  kFromController,
  kFromHost,
};

// Must match the number of Direction enumerators.
inline constexpr size_t kNumDirections = 2;
static_assert(kNumDirections == static_cast<size_t>(Direction::kFromHost) + 1);

// Returns string description of the direction. Useful for logging.
constexpr const char* DirectionToString(Direction direction) {
  switch (direction) {
    case Direction::kFromController:
      return "from controller";
    case Direction::kFromHost:
      return "from host";
  }
}

constexpr Direction FlipDirection(Direction direction) {
  switch (direction) {
    case Direction::kFromController:
      return Direction::kFromHost;
    case Direction::kFromHost:
      return Direction::kFromController;
  }
}

}  // namespace pw::bluetooth::proxy
