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
#include "pw_status/try.h"

namespace pw::fuzzer::examples {
namespace {
Metric::Key Hash(std::string_view str) {
  PW_CHECK(std::all_of(str.begin(), str.end(), isprint));
  return static_cast<Metric::Key>(std::hash<std::string_view>{}(str));
}

template <typename T>
using IsCopyable =
    std::enable_if_t<std::is_unsigned_v<T> && !std::is_same_v<T, bool>>;

template <typename T, typename = IsCopyable<T>>
Status CopyTo(pw::ByteSpan dst, size_t& offset, const T& src) {
  size_t len = sizeof(src);
  if (offset + len > dst.size()) {
    return Status::ResourceExhausted();
  }
  memcpy(&dst[offset], &src, len);
  offset += len;
  return OkStatus();
}

template <typename T, typename = IsCopyable<T>>
Status CopyFrom(pw::ConstByteSpan src, size_t& offset, T& dst) {
  size_t len = sizeof(dst);
  if (offset + len > src.size()) {
    return Status::ResourceExhausted();
  }
  memcpy(&dst, &src[offset], len);
  offset += len;
  return OkStatus();
}

}  // namespace

Metric::Metric(std::string_view name_, Value value_)
    : name(name_), value(value_) {
  key = Hash(name_);
}

std::optional<Metric::Value> Metrics::GetValue(std::string_view name) const {
  for (const auto& metric : metrics_) {
    if (metric.name == name) {
      return metric.value;
    }
  }
  return std::optional<Metric::Value>();
}

Status Metrics::SetValue(std::string_view name, Metric::Value value) {
  for (auto& metric : metrics_) {
    if (metric.name == name) {
      metric.value = value;
      return OkStatus();
    }
  }
  if (metrics_.full()) {
    return Status::ResourceExhausted();
  }
  metrics_.emplace_back(name, value);
  return OkStatus();
}

const Vector<Metric>& Metrics::GetMetrics() const { return metrics_; }

Status Metrics::SetMetrics(const Vector<Metric>& metrics) {
  if (metrics_.capacity() < metrics.size()) {
    return Status::ResourceExhausted();
  }
  metrics_.assign(metrics.begin(), metrics.end());
  return OkStatus();
}

StatusWithSize Metrics::Serialize(pw::ByteSpan buffer) const {
  size_t offset = 0;
  PW_TRY_WITH_SIZE(CopyTo(buffer, offset, metrics_.size()));
  for (const auto& metric : metrics_) {
    PW_TRY_WITH_SIZE(CopyTo(buffer, offset, metric.key));
    PW_TRY_WITH_SIZE(CopyTo(buffer, offset, metric.value));
  }
  return StatusWithSize(offset);
}

Status Metrics::Deserialize(pw::ConstByteSpan buffer) {
  size_t offset = 0;
  size_t num_values = 0;
  PW_TRY(CopyFrom(buffer, offset, num_values));
  for (size_t i = 0; i < num_values; ++i) {
    Metric::Key key;
    PW_TRY(CopyFrom(buffer, offset, key));
    Metric::Value value;
    PW_TRY(CopyFrom(buffer, offset, value));
    bool found = false;
    for (auto& metric : metrics_) {
      if (metric.key == key) {
        metric.value = value;
        found = true;
        break;
      }
    }
    if (!found) {
      return Status::InvalidArgument();
    }
  }
  return OkStatus();
}

}  // namespace pw::fuzzer::examples
