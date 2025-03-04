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

#include "pw_metric/metric.h"

#include <array>
#include <atomic>
#include <limits>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_span/span.h"
#include "pw_tokenizer/base64.h"

namespace pw::metric {
namespace {

template <typename T>
span<const std::byte> AsSpan(const T& t) {
  return span<const std::byte>(reinterpret_cast<const std::byte*>(&t),
                               sizeof(t));
}

// A convenience class to encode a token as base64 while managing the storage.
// TODO(keir): Consider putting this into upstream pw_tokenizer.
struct Base64EncodedToken {
  Base64EncodedToken(Token token) {
    size_t encoded_size = tokenizer::PrefixedBase64Encode(AsSpan(token), data);
    data[encoded_size] = 0;
  }

  const char* value() { return data.data(); }
  std::array<char, 16> data;
};

const char* Indent(int level) {
  static const char* kWhitespace10 = "          ";
  level = std::min(level, 4);
  return kWhitespace10 + 8 - 2 * level;
}

}  // namespace

// Enable easier registration when used as a member.
Metric::Metric(Token name, float value, IntrusiveList<Metric>& metrics)
    : Metric(name, value) {
  metrics.push_front(*this);
}
Metric::Metric(Token name, uint32_t value, IntrusiveList<Metric>& metrics)
    : Metric(name, value) {
  metrics.push_front(*this);
}

float Metric::as_float() const {
  PW_DCHECK(is_float());
  return float_.load(std::memory_order_relaxed);
}

uint32_t Metric::as_int() const {
  PW_DCHECK(is_int());
  return uint_.load(std::memory_order_relaxed);
}

void Metric::Increment(uint32_t amount) {
  PW_DCHECK(is_int());

  uint32_t value = uint_.load();
  uint32_t updated;

  if (value == std::numeric_limits<uint32_t>::max()) {
    return;
  }

  do {
    if (PW_ADD_OVERFLOW(value, amount, &updated)) {
      updated = std::numeric_limits<uint32_t>::max();
    }
  } while (!uint_.compare_exchange_weak(value, updated));
}

void Metric::Decrement(uint32_t amount) {
  PW_DCHECK(is_int());

  uint32_t value = uint_.load();
  uint32_t updated;

  do {
    if (value == 0) {
      return;
    }

    if (PW_SUB_OVERFLOW(value, amount, &updated)) {
      updated = 0;
    }
  } while (!uint_.compare_exchange_weak(value, updated));
}

void Metric::SetInt(uint32_t value) {
  PW_DCHECK(is_int());
  uint_.store(value, std::memory_order_relaxed);
}

void Metric::SetFloat(float value) {
  PW_DCHECK(is_float());
  float_.store(value, std::memory_order_relaxed);
}

void Metric::Dump(int level, bool last) const {
  Base64EncodedToken encoded_name(name());
  const char* indent = Indent(level);
  const char* comma = last ? "" : ",";
  if (is_float()) {
    // Variadic macros promote float to double. Explicitly cast here to
    // acknowledge this and allow projects to use -Wdouble-promotion.
    PW_LOG_INFO("%s \"%s\": %f%s",
                indent,
                encoded_name.value(),
                static_cast<double>(as_float()),
                comma);
  } else {
    PW_LOG_INFO("%s \"%s\": %u%s",
                indent,
                encoded_name.value(),
                static_cast<unsigned int>(as_int()),
                comma);
  }
}

void Metric::Dump(const IntrusiveList<Metric>& metrics, int level) {
  auto iter = metrics.begin();
  while (iter != metrics.end()) {
    const Metric& m = *iter++;
    m.Dump(level, iter == metrics.end());
  }
}

Group::Group(Token name, IntrusiveList<Group>& groups) : name_(name) {
  groups.push_front(*this);
}

void Group::Dump() const {
  PW_LOG_INFO("{");
  Dump(0, true);
  PW_LOG_INFO("}");
}

void Group::Dump(int level, bool last) const {
  Base64EncodedToken encoded_name(name());
  const char* indent = Indent(level);
  const char* comma = last ? "" : ",";
  PW_LOG_INFO("%s\"%s\": {", indent, encoded_name.value());
  Group::Dump(children(), level + 1);
  Metric::Dump(metrics(), level + 1);
  PW_LOG_INFO("%s}%s", indent, comma);
}

void Group::Dump(const IntrusiveList<Group>& groups, int level) {
  auto iter = groups.begin();
  while (iter != groups.end()) {
    const Group& g = *iter++;
    g.Dump(level, iter == groups.end());
  }
}

}  // namespace pw::metric
