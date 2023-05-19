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

#include "metrics.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "pw_assert/check.h"

namespace pw_fuzzer::examples {
namespace {
Metrics::Key Hash(std::string_view str) {
  PW_CHECK(std::all_of(str.begin(), str.end(), isprint));
  return static_cast<Metrics::Key>(std::hash<std::string_view>{}(str));
}

template <typename T>
using IsCopyable =
    std::enable_if_t<std::is_unsigned_v<T> && !std::is_same_v<T, bool>>;

template <typename T, typename = IsCopyable<T>>
void CopyTo(pw::ByteSpan dst, size_t& offset, const T& src) {
  size_t len = sizeof(src);
  if (offset + len <= dst.size()) {
    memcpy(&dst[offset], &src, len);
  }
  offset += len;
}

template <typename T, typename = IsCopyable<T>>
void CopyFrom(pw::ConstByteSpan src, size_t& offset, T& dst) {
  size_t len = sizeof(dst);
  if (offset + len <= src.size()) {
    memcpy(&dst, &src[offset], len);
  }
  offset += len;
}

}  // namespace

std::optional<Metrics::Value> Metrics::GetValue(std::string_view name) const {
  auto iter = values_.find(std::string(name));
  if (iter == values_.end()) {
    return std::optional<Metrics::Value>();
  }
  return iter->second;
}

void Metrics::SetValue(std::string_view name, Value value) {
  std::string name_copy(name);
  auto iter = values_.find(name_copy);
  if (iter == values_.end()) {
    AddKey(name, Hash(name));
    values_[name] = value;
  } else {
    iter->second = value;
  }
}

const Metrics::KeyMap& Metrics::GetKeys() const { return keys_; }

void Metrics::SetKeys(const Metrics::KeyMap& keys) {
  for (const auto& [name, key] : keys) {
    AddKey(name, key);
    values_.try_emplace(name, 0);
  }
}

size_t Metrics::Serialize(pw::ByteSpan buffer) const {
  size_t offset = 0;
  CopyTo(buffer, offset, values_.size());
  for (const auto& [name, key] : keys_) {
    CopyTo(buffer, offset, key);
    CopyTo(buffer, offset, values_.at(name));
  }
  return offset;
}

bool Metrics::Deserialize(pw::ConstByteSpan buffer) {
  size_t offset = 0;
  size_t num_values = 0;
  CopyFrom(buffer, offset, num_values);
  for (size_t i = 0; i < num_values; ++i) {
    Metrics::Key key;
    CopyFrom(buffer, offset, key);
    Value value;
    CopyFrom(buffer, offset, value);
    if (offset > buffer.size()) {
      return false;
    }
    auto iter = names_.find(key);
    if (iter != names_.end()) {
      auto [mapped_key, name] = *iter;
      values_[name] = value;
    }
  }
  return offset <= buffer.size();
}

void Metrics::AddKey(std::string_view name, Metrics::Key key) {
  auto [iter, found] = names_.emplace(key, name);
  auto& [mapped_key, mapped_name] = *iter;
  name = std::string_view(mapped_name);
  keys_[name] = mapped_key;
}

}  // namespace pw_fuzzer::examples
