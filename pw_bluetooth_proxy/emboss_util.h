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

#include <type_traits>

#include "pw_span/span.h"

/// Creates a pw::span on the HCI portion of an H4 buffer.
template <typename C>
constexpr inline auto H4HciSubspan(C&& container) {
  return pw::span(container.data() + 1, container.size() - 1);
}

// Create an Emboss View or Writer from a pw::span value or reference. The
// Emboss type is determined by the template's first parameter.
// Unlike the Emboss `Make*View` creation methods, this function accepts a
// reference so it can be used with rvalues. This is ok to do with pw::span
// since it doesn't own its underlying data. Another key difference is the
// caller explicitly chooses if they want a View or a Writer using the first
// template argument.
template <
    typename EmbossT,
    typename ContainerT,
    typename = std::enable_if<std::is_base_of_v<pw::span<uint8_t>, ContainerT>>>
constexpr inline EmbossT MakeEmboss(ContainerT&& buffer) {
  return EmbossT(buffer.data(), buffer.size());
}
