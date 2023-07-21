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
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_string/string.h"

namespace pw::fuzzer::examples {

// DOCSTAG: [pwfuzzer_examples_fuzztest-metrics_h]
// Represent a named value. In order to transmit these values efficiently, they
// can be referenced by fixed length, generated keys instead of names.
struct Metric {
  using Key = uint16_t;
  using Value = uint32_t;

  static constexpr size_t kMaxNameLen = 32;

  Metric() = default;
  Metric(std::string_view name_, Value value_);

  InlineString<kMaxNameLen> name;
  Key key = 0;
  Value value = 0;
};

// Represents a set of measurements from a particular source.
//
// In order to transmit metrics efficiently, the names of metrics are hashed
// internally into fixed length keys. The names can be shared once via `GetKeys`
// and `SetKeys`, after which metrics can be efficiently shared via `Serialize`
// and `Deserialize`.
class Metrics {
 public:
  static constexpr size_t kMaxMetrics = 32;
  static constexpr size_t kMaxSerializedSize =
      sizeof(size_t) +
      kMaxMetrics * (sizeof(Metric::Key) + sizeof(Metric::Value));

  // Retrieves the value of a named metric and stores it in `out_value`. The
  // name must consist of printable ASCII characters. Returns false if the named
  // metric was not `Set` or `Import`ed.
  std::optional<Metric::Value> GetValue(std::string_view name) const;

  // Sets the value of a named metric. The name must consist of printable ASCII
  // characters, and will be added to the mapping of names to keys.
  Status SetValue(std::string_view name, Metric::Value value);

  // Returns the current mapping of names to keys.
  const Vector<Metric>& GetMetrics() const;

  // Replaces the current mapping of names to keys.
  Status SetMetrics(const Vector<Metric>& metrics);

  // Serializes this object to the given `buffer`. Does not write more bytes
  // than `buffer.size()`. Returns the number of number of bytes written or an
  // error if insufficient space.
  StatusWithSize Serialize(pw::ByteSpan buffer) const;

  // Populates this object from the data in the given `buffer`.
  // Returns whether this buffer could be deserialized.
  Status Deserialize(pw::ConstByteSpan buffer);

 private:
  Vector<Metric, kMaxMetrics> metrics_;
};
// DOCSTAG: [pwfuzzer_examples_fuzztest-metrics_h]

}  // namespace pw::fuzzer::examples
