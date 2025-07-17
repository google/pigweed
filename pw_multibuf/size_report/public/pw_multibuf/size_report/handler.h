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

#include <type_traits>

#include "pw_async2/task.h"
#include "pw_bytes/span.h"
#include "pw_checksum/crc32.h"
#include "pw_multibuf/examples/protocol.h"

namespace pw::multibuf::size_report {

namespace internal {

template <typename MultiBufType, typename = void>
struct is_multibuf_instance : std::false_type {};

template <typename MultiBufType>
struct is_multibuf_instance<
    MultiBufType,
    std::void_t<decltype(std::declval<MultiBufType&>()->begin())>>
    : std::true_type {};

template <typename MultiBufType>
constexpr auto& GetMultiBuf(MultiBufType& mb) {
  if constexpr (is_multibuf_instance<MultiBufType>::value) {
    return *mb;
  } else {
    return mb;
  }
}

template <typename MultiBufType>
constexpr const auto& GetMultiBuf(const MultiBufType& mb) {
  if constexpr (is_multibuf_instance<MultiBufType>::value) {
    return *mb;
  } else {
    return mb;
  }
}

template <typename MultiBufType>
using MultiBufInterface = std::remove_reference_t<decltype(GetMultiBuf(
    std::declval<MultiBufType&>()))>;

}  // namespace internal

template <typename MultiBufType>
class FrameHandler : public async2::Task {
 public:
  using const_iterator =
      typename internal::MultiBufInterface<MultiBufType>::const_iterator;

 protected:
  MultiBufType AllocateFrame() { return DoAllocateFrame(); }

  void Truncate(MultiBufType& mb, size_t length) {
    PW_ASSERT(length <= internal::GetMultiBuf(mb).size());
    DoTruncate(mb, length);
  }

  void Narrow(MultiBufType& mb, size_t offset, size_t length = dynamic_extent) {
    DoNarrow(mb, offset, length);
  }

  void Widen(MultiBufType& mb, size_t prefix_len, size_t suffix_len = 0) {
    DoWiden(mb, prefix_len, suffix_len);
  }

  uint32_t CalculateChecksum(const MultiBufType& mb);

  void PushBack(MultiBufType& mb, MultiBufType&& chunk) {
    DoPushBack(mb, std::move(chunk));
  }

 private:
  virtual MultiBufType DoAllocateFrame() = 0;

  virtual void DoTruncate(MultiBufType& mb, size_t offset) = 0;
  virtual void DoNarrow(MultiBufType& mb, size_t offset, size_t length) = 0;
  virtual void DoWiden(MultiBufType& mb,
                       size_t prefix_len,
                       size_t suffix_len) = 0;
  virtual void DoPushBack(MultiBufType& mb, MultiBufType&& chunk) = 0;

  virtual const_iterator GetBegin(const MultiBufType& mb) const = 0;
  virtual const_iterator GetEnd(const MultiBufType& mb) const = 0;
};

// Template method implementations.

template <typename MultiBufType>
uint32_t FrameHandler<MultiBufType>::CalculateChecksum(const MultiBufType& mb) {
  auto& mbi = internal::GetMultiBuf(mb);
  PW_ASSERT(mbi.size() >= sizeof(examples::DemoLinkFooter));
  size_t remaining = mbi.size() - sizeof(examples::DemoLinkFooter);
  checksum::Crc32 crc32;
  for (auto iter = GetBegin(mb); iter != GetEnd(mb); ++iter, --remaining) {
    if (remaining == 0) {
      break;
    }
    crc32.Update(*iter);
  }
  return crc32.value();
}

}  // namespace pw::multibuf::size_report
