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
#include <string>
#include <string_view>
#include <unordered_map>

#include "pw_bytes/span.h"

namespace pw_fuzzer::examples {

// DOCSTAG: [pwfuzzer_examples_fuzztest-metrics_h]
// Represents a set of measurements from a particular source.
//
// In order to transmit metrics efficiently, the names of metrics are hashed
// internally into fixed length keys. The names can be shared once via `GetKeys`
// and `SetKeys`, after which metrics can be efficiently shared via `Serialize`
// and `Deserialize`.
class Metrics {
 public:
  using Key = uint16_t;
  using KeyMap = std::unordered_map<std::string_view, Key>;
  using Value = uint32_t;

  // Retrieves the value of a named metric and stores it in `out_value`. The
  // name must consist of printable ASCII characters. Returns false if the named
  // metric was not `Set` or `Import`ed.
  std::optional<Value> GetValue(std::string_view name) const;

  // Sets the value of a named metric. The name must consist of printable ASCII
  // characters, and will be added to the mapping of names to keys.
  void SetValue(std::string_view name, Value value);

  // Returns the current mapping of names to keys.
  const KeyMap& GetKeys() const;

  // Replaces the current mapping of names to keys.
  void SetKeys(const KeyMap& keys);

  // Serializes this object to the given `buffer`. Does not write more bytes
  // than `buffer.size()`. Returns the number of number of bytes written if the
  // buffer is large enough, or the number that would have been if the `buffer`
  // had been large enough.
  size_t Serialize(pw::ByteSpan buffer) const;

  // Populates this object from the data in the given `buffer`.
  // Returns whether this buffer could be deserialized.
  bool Deserialize(pw::ConstByteSpan buffer);

 private:
  using NameMap = std::unordered_map<Key, std::string>;
  using ValueMap = std::unordered_map<std::string_view, Value>;

  // Add a name and a keys, while managing the associated `string_view`s.
  void AddKey(std::string_view name, Key key);

  // The data represented by this object are the names mapped to values in
  // `values_`. In order to marshal this data efficiently, the names are mapped
  // to fixed length keys in `keys_`. In order to efficently unmarshal, these
  // keys are mapped back to names in `names_`.
  NameMap names_;
  KeyMap keys_;
  ValueMap values_;
};
// DOCSTAG: [pwfuzzer_examples_fuzztest-metrics_h]

}  // namespace pw_fuzzer::examples
