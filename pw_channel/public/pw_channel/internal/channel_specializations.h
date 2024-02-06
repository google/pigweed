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

#include "pw_channel/channel.h"
#include "pw_toolchain/internal/sibling_cast.h"

namespace pw::channel {
namespace internal {

// Defines conversions between compatible channel types.
template <typename Self>
class Conversions {
 private:
  template <typename Sibling>
  static constexpr void ConversionIsValid() {
    static_assert(Sibling::kDataType == Self::kDataType,
                  "Datagram and byte channels are not interchangeable");
    static_assert(!Sibling::kReliable || Self::kReliable,
                  "Cannot use a unreliable channel as a reliable channel");
    static_assert(!Sibling::kReadable || Self::kReadable,
                  "Cannot use a non-readable channel as a readable channel");
    static_assert(!Sibling::kWritable || Self::kWritable,
                  "Cannot use a non-writable channel as a writable channel");
    static_assert(!Sibling::kSeekable || Self::kSeekable,
                  "Cannot use a non-seekable channel as a seekable channel");
  }

 public:
  template <typename Sibling>
  [[nodiscard]] Sibling& as() {
    if constexpr (std::is_same_v<Sibling, Channel>) {
      return static_cast<Self&>(*this);
    } else {
      ConversionIsValid<Sibling>();
      return pw::internal::SiblingCast<Sibling&, Channel>(
          static_cast<Self&>(*this));
    }
  }

  template <typename Sibling>
  [[nodiscard]] const Sibling& as() const {
    if constexpr (std::is_same_v<Sibling, Channel>) {
      return static_cast<const Self&>(*this);
    } else {
      ConversionIsValid<Sibling>();
      return pw::internal::SiblingCast<const Sibling&, Channel>(
          static_cast<const Self&>(*this));
    }
  }

  template <typename Sibling, auto = ConversionIsValid<Sibling>()>
  operator Sibling&() {
    return as<Sibling>();
  }

  template <typename Sibling, auto = ConversionIsValid<Sibling>()>
  operator const Sibling&() const {
    return as<Sibling>();
  }
};

}  // namespace internal

// Defines a channel specialization with the specified type and read/write/seek
// capabilities. This macro expands other macros to implement unsupported
// operations and hide them from the public API.
//
// ByteChannel and DatagramChannel are specialized for each supported
// combination of read/write/seek attributes. Invalid combinations fallback to
// the default implementation and fail a static_assert.
//
// Specializing these channel classes accomplishes the following:
//
// - Implement unsupported operations in a standard way. Extending a channel
//   only requires implementing supported functions.
// - Hide unsupported overloads or functions from the public API. They are still
//   accessible in the Channel base class.
//
#define _PW_CHANNEL(type, read, write, seek, ...)                        \
  template <>                                                            \
  class type##Channel<__VA_ARGS__>                                       \
      : public Channel,                                                  \
        public internal::Conversions<type##Channel<__VA_ARGS__>> {       \
   private:                                                              \
    static_assert(PropertiesAreValid<__VA_ARGS__>());                    \
                                                                         \
    static constexpr uint8_t kProperties = GetProperties<__VA_ARGS__>(); \
                                                                         \
    _PW_CHANNEL_READABLE_##read;                                         \
    _PW_CHANNEL_WRITABLE_##write;                                        \
    _PW_CHANNEL_SEEKABLE_##seek;                                         \
                                                                         \
   public:                                                               \
    static constexpr DataType kDataType = DataType::k##type;             \
    static constexpr bool kReliable =                                    \
        (kProperties & Property::kReliable) != 0;                        \
    static constexpr bool kSeekable =                                    \
        (kProperties & Property::kSeekable) != 0;                        \
    static constexpr bool kReadable =                                    \
        (kProperties & Property::kReadable) != 0;                        \
    static constexpr bool kWritable =                                    \
        (kProperties & Property::kWritable) != 0;                        \
                                                                         \
    _PW_CHANNEL_DISABLE_POLL_OVERLOAD_##type##read;                      \
                                                                         \
   protected:                                                            \
    constexpr type##Channel()                                            \
        : Channel(channel::DataType::k##type, kProperties) {}            \
  }

// Macros that hide the PollRead max_bytes overload for datagram channels.
#define _PW_CHANNEL_DISABLE_POLL_OVERLOAD_DatagramREAD                         \
  async2::Poll<Result<multibuf::MultiBuf>> PollRead(async2::Context& cx) {     \
    return Channel::PollRead(cx);                                              \
  }                                                                            \
  template <typename OverloadDisabled = void>                                  \
  async2::Poll<Result<multibuf::MultiBuf>> PollRead(async2::Context&,          \
                                                    size_t) {                  \
    static_assert(!std::is_same_v<OverloadDisabled, OverloadDisabled>,         \
                  "The PollRead overload with a max_bytes argument is not "    \
                  "supported for DatagramChannels");                           \
    return async2::Ready(Result<multibuf::MultiBuf>(Status::Unimplemented())); \
  }                                                                            \
  static_assert(true)

#define _PW_CHANNEL_DISABLE_POLL_OVERLOAD_DatagramSKIP static_assert(true)
#define _PW_CHANNEL_DISABLE_POLL_OVERLOAD_ByteSKIP static_assert(true)
#define _PW_CHANNEL_DISABLE_POLL_OVERLOAD_ByteREAD static_assert(true)

// Macros that stub out read/write/seek and hide them if unsupported.
#define _PW_CHANNEL_READABLE_READ static_assert(true)
#define _PW_CHANNEL_READABLE_SKIP                                              \
  async2::Poll<Result<multibuf::MultiBuf>> DoPollRead(async2::Context&,        \
                                                      size_t) final {          \
    return async2::Ready(Result<multibuf::MultiBuf>(Status::Unimplemented())); \
  }                                                                            \
  using Channel::PollRead

#define _PW_CHANNEL_WRITABLE_WRTE static_assert(true)
#define _PW_CHANNEL_WRITABLE_SKIP                                        \
  async2::Poll<> DoPollReadyToWrite(async2::Context&) final {            \
    return async2::Ready(); /* Should this be Ready() correct here? */   \
  }                                                                      \
  Result<channel::WriteToken> DoWrite(multibuf::MultiBuf&&) final {      \
    return Status::Unimplemented();                                      \
  }                                                                      \
  async2::Poll<Result<WriteToken>> DoPollFlush(async2::Context&) final { \
    return async2::Ready(Result<WriteToken>(Status::Unimplemented()));   \
  }                                                                      \
  using Channel::PollReadyToWrite;                                       \
  using Channel::Write;                                                  \
  using Channel::PollFlush

#define _PW_CHANNEL_SEEKABLE_SEEK static_assert(true)
// TODO: b/323622630 - Implement DoSeek() and DoPosition()
#define _PW_CHANNEL_SEEKABLE_SKIP \
  using Channel::Seek;            \
  using Channel::Position

// Generate specializations for the supported channel types.
// _PW_CHANNEL(Byte, READ, WRTE, SEEK, kReliable, kReadable, kWritable,
// kSeekable);
_PW_CHANNEL(Byte, READ, WRTE, SKIP, kReliable, kReadable, kWritable);
// _PW_CHANNEL(Byte, READ, SKIP, SEEK, kReliable, kReadable, kSeekable);
_PW_CHANNEL(Byte, READ, SKIP, SKIP, kReliable, kReadable);

// _PW_CHANNEL(Byte, READ, WRTE, SEEK, kReadable, kWritable, kSeekable);
_PW_CHANNEL(Byte, READ, WRTE, SKIP, kReadable, kWritable);
// _PW_CHANNEL(Byte, READ, SKIP, SEEK, kReadable, kSeekable);
_PW_CHANNEL(Byte, READ, SKIP, SKIP, kReadable);

// _PW_CHANNEL(Byte, SKIP, WRTE, SEEK, kReliable, kWritable, kSeekable);
_PW_CHANNEL(Byte, SKIP, WRTE, SKIP, kReliable, kWritable);
// _PW_CHANNEL(Byte, SKIP, WRTE, SEEK, kWritable, kSeekable);
_PW_CHANNEL(Byte, SKIP, WRTE, SKIP, kWritable);

// _PW_CHANNEL(
//     Datagram, READ, WRTE, SEEK, kReliable, kReadable, kWritable, kSeekable);
_PW_CHANNEL(Datagram, READ, WRTE, SKIP, kReliable, kReadable, kWritable);
// _PW_CHANNEL(Datagram, READ, SKIP, SEEK, kReliable, kReadable, kSeekable);
_PW_CHANNEL(Datagram, READ, SKIP, SKIP, kReliable, kReadable);

// _PW_CHANNEL(Datagram, READ, WRTE, SEEK, kReadable, kWritable, kSeekable);
_PW_CHANNEL(Datagram, READ, WRTE, SKIP, kReadable, kWritable);
// _PW_CHANNEL(Datagram, READ, SKIP, SEEK, kReadable, kSeekable);
_PW_CHANNEL(Datagram, READ, SKIP, SKIP, kReadable);

// _PW_CHANNEL(Datagram, SKIP, WRTE, SEEK, kReliable, kWritable, kSeekable);
_PW_CHANNEL(Datagram, SKIP, WRTE, SKIP, kReliable, kWritable);
// _PW_CHANNEL(Datagram, SKIP, WRTE, SEEK, kWritable, kSeekable);
_PW_CHANNEL(Datagram, SKIP, WRTE, SKIP, kWritable);

#undef _PW_CHANNEL_DISABLE_POLL_OVERLOAD_DatagramREAD
#undef _PW_CHANNEL_DISABLE_POLL_OVERLOAD_DatagramSKIP
#undef _PW_CHANNEL_DISABLE_POLL_OVERLOAD_ByteSKIP
#undef _PW_CHANNEL_DISABLE_POLL_OVERLOAD_ByteREAD
#undef _PW_CHANNEL_READABLE_READ
#undef _PW_CHANNEL_READABLE_SKIP
#undef _PW_CHANNEL_WRITABLE_WRTE
#undef _PW_CHANNEL_WRITABLE_SKIP
#undef _PW_CHANNEL_SEEKABLE_SEEK
#undef _PW_CHANNEL_SEEKABLE_SKIP
#undef _PW_CHANNEL

}  // namespace pw::channel
