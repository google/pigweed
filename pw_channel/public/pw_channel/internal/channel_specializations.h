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

// Returns whether a sibling channel supports the required properties.
template <typename Self, typename Sibling>
using EnableIfConversionIsValid =
    std::enable_if_t<  // Sibling type must be a channel
        std::is_base_of_v<AnyChannel, Sibling> &&
        // Datagram and byte channels are not interchangeable
        (Sibling::data_type() == Self::data_type()) &&
        // Cannot use a unreliable channel as a reliable channel
        (!Sibling::reliable() || Self::reliable()) &&
        // Cannot use a non-readable channel as a readable channel
        (!Sibling::readable() || Self::readable()) &&
        // Cannot use a non-writable channel as a writable channel
        (!Sibling::writable() || Self::writable()) &&
        // Cannot use a non-seekable channel as a seekable channel
        (!Sibling::seekable() || Self::seekable())>;

// Defines conversions between compatible channel types.
template <typename Self>
class Conversions {
 private:
  // Performs the same checks as EnableIfConversionIsValid, but generates a
  // static_assert with a helpful message if any condition is not met.
  template <typename Sibling>
  static constexpr void CheckThatConversionIsValid() {
    static_assert(std::is_base_of_v<AnyChannel, Sibling>,
                  "Only conversions to other AnyChannel types are supported");
    static_assert(Sibling::data_type() == Self::data_type(),
                  "Datagram and byte channels are not interchangeable");
    static_assert(!Sibling::reliable() || Self::reliable(),
                  "Cannot use a unreliable channel as a reliable channel");
    static_assert(!Sibling::readable() || Self::readable(),
                  "Cannot use a non-readable channel as a readable channel");
    static_assert(!Sibling::writable() || Self::writable(),
                  "Cannot use a non-writable channel as a writable channel");
    static_assert(!Sibling::seekable() || Self::seekable(),
                  "Cannot use a non-seekable channel as a seekable channel");
  }

 public:
  // Explicit conversion to a compatible AnyChannel type.
  template <typename Sibling>
  [[nodiscard]] Sibling& as() {
    if constexpr (std::is_same_v<Sibling, AnyChannel>) {
      return static_cast<Self&>(*this);
    } else {
      CheckThatConversionIsValid<Sibling>();
      return pw::internal::SiblingCast<Sibling&, AnyChannel>(
          static_cast<Self&>(*this));
    }
  }
  template <typename Sibling>
  [[nodiscard]] const Sibling& as() const {
    if constexpr (std::is_same_v<Sibling, AnyChannel>) {
      return static_cast<const Self&>(*this);
    } else {
      CheckThatConversionIsValid<Sibling>();
      return pw::internal::SiblingCast<const Sibling&, AnyChannel>(
          static_cast<const Self&>(*this));
    }
  }

  // Explicit conversion to a ByteChannel or DatagramChannel with the specified
  // properties.
  template <Property... kProperties>
  [[nodiscard]] auto& as() {
    using Sibling = Channel<Self::data_type(), kProperties...>;
    CheckThatConversionIsValid<Sibling>();
    return pw::internal::SiblingCast<Sibling&, AnyChannel>(
        static_cast<Self&>(*this));
  }
  template <Property... kProperties>
  [[nodiscard]] const auto& as() const {
    using Sibling = Channel<Self::data_type(), kProperties...>;
    CheckThatConversionIsValid<Sibling>();
    return pw::internal::SiblingCast<const Sibling&, AnyChannel>(
        static_cast<const Self&>(*this));
  }
};

}  // namespace internal

// Defines a channel specialization with the specified type and read/write/seek
// capabilities. This macro expands other macros to implement unsupported
// operations and hide them from the public API.
//
// Channel is specialized for each supported combination of byte/datagram and
// read/write/seek attributes. Invalid combinations fall back to the default
// implementation and fail a static_assert.
//
// Specializing these channel classes accomplishes the following:
//
// - Implement unsupported operations in a standard way. Extending a channel
//   only requires implementing supported functions.
// - Hide unsupported overloads or functions from the public API. They are still
//   accessible in the AnyChannel base class.
//
#define _PW_CHANNEL(type, read, write, seek, ...)                             \
  template <>                                                                 \
  class Channel<DataType::type, __VA_ARGS__>                                  \
      : public AnyChannel,                                                    \
        public internal::Conversions<Channel<DataType::type, __VA_ARGS__>> {  \
   private:                                                                   \
    static_assert(PropertiesAreValid<__VA_ARGS__>());                         \
                                                                              \
    static constexpr uint8_t kProperties = GetProperties<__VA_ARGS__>();      \
                                                                              \
    _PW_CHANNEL_READABLE_##read;                                              \
    _PW_CHANNEL_WRITABLE_##write;                                             \
    _PW_CHANNEL_SEEKABLE_##seek;                                              \
                                                                              \
   public:                                                                    \
    [[nodiscard]] static constexpr DataType data_type() {                     \
      return DataType::type;                                                  \
    }                                                                         \
    [[nodiscard]] static constexpr bool reliable() {                          \
      return (kProperties & Property::kReliable) != 0;                        \
    }                                                                         \
    [[nodiscard]] static constexpr bool seekable() {                          \
      return (kProperties & Property::kSeekable) != 0;                        \
    }                                                                         \
    [[nodiscard]] static constexpr bool readable() {                          \
      return (kProperties & Property::kReadable) != 0;                        \
    }                                                                         \
    [[nodiscard]] static constexpr bool writable() {                          \
      return (kProperties & Property::kWritable) != 0;                        \
    }                                                                         \
                                                                              \
    /* Implicit conversion to a compatible channel type. */                   \
    template <                                                                \
        typename Sibling,                                                     \
        typename = internal::EnableIfConversionIsValid<Channel, Sibling>>     \
    operator Sibling&() {                                                     \
      return as<Sibling>();                                                   \
    }                                                                         \
    template <                                                                \
        typename Sibling,                                                     \
        typename = internal::EnableIfConversionIsValid<Channel, Sibling>>     \
    operator const Sibling&() const {                                         \
      return as<Sibling>();                                                   \
    }                                                                         \
                                                                              \
    _PW_CHANNEL_##type(__VA_ARGS__);                                          \
                                                                              \
   protected:                                                                 \
    constexpr Channel() : AnyChannel(channel::DataType::type, kProperties) {} \
  }

// Macros that stub out read/write/seek and hide them if unsupported.
#define _PW_CHANNEL_READABLE_READ static_assert(true)
#define _PW_CHANNEL_READABLE_SKIP                                              \
  async2::Poll<Result<multibuf::MultiBuf>> DoPollRead(async2::Context&)        \
      final {                                                                  \
    return async2::Ready(Result<multibuf::MultiBuf>(Status::Unimplemented())); \
  }                                                                            \
  using AnyChannel::PollRead

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
  using AnyChannel::PollReadyToWrite;                                    \
  using AnyChannel::Write;                                               \
  using AnyChannel::PollFlush

#define _PW_CHANNEL_SEEKABLE_SEEK static_assert(true)
// TODO: b/323622630 - Implement DoSeek() and DoPosition()
#define _PW_CHANNEL_SEEKABLE_SKIP \
  using AnyChannel::Seek;         \
  using AnyChannel::Position

// Function for converting datagram channels to byte channels.
#define _PW_CHANNEL_kDatagram(...)                                           \
  ByteChannel<__VA_ARGS__>& IgnoreDatagramBoundaries() {                     \
    return pw::internal::SiblingCast<ByteChannel<__VA_ARGS__>&, AnyChannel>( \
        *this);                                                              \
  }                                                                          \
  const ByteChannel<__VA_ARGS__>& IgnoreDatagramBoundaries() const {         \
    return pw::internal::SiblingCast<const ByteChannel<__VA_ARGS__>&,        \
                                     AnyChannel>(*this);                     \
  }                                                                          \
  static_assert(true)

#define _PW_CHANNEL_kByte(...) static_assert(true)

// Generate specializations for the supported channel types.
// _PW_CHANNEL(
//     kByte, READ, WRTE, SEEK, kReliable, kReadable, kWritable, kSeekable);
_PW_CHANNEL(kByte, READ, WRTE, SKIP, kReliable, kReadable, kWritable);
// _PW_CHANNEL(kByte, READ, SKIP, SEEK, kReliable, kReadable, kSeekable);
_PW_CHANNEL(kByte, READ, SKIP, SKIP, kReliable, kReadable);

// _PW_CHANNEL(kByte, READ, WRTE, SEEK, kReadable, kWritable, kSeekable);
_PW_CHANNEL(kByte, READ, WRTE, SKIP, kReadable, kWritable);
// _PW_CHANNEL(kByte, READ, SKIP, SEEK, kReadable, kSeekable);
_PW_CHANNEL(kByte, READ, SKIP, SKIP, kReadable);

// _PW_CHANNEL(kByte, SKIP, WRTE, SEEK, kReliable, kWritable, kSeekable);
_PW_CHANNEL(kByte, SKIP, WRTE, SKIP, kReliable, kWritable);
// _PW_CHANNEL(kByte, SKIP, WRTE, SEEK, kWritable, kSeekable);
_PW_CHANNEL(kByte, SKIP, WRTE, SKIP, kWritable);

// _PW_CHANNEL(
//     kDatagram, READ, WRTE, SEEK, kReliable, kReadable, kWritable, kSeekable);
_PW_CHANNEL(kDatagram, READ, WRTE, SKIP, kReliable, kReadable, kWritable);
// _PW_CHANNEL(kDatagram, READ, SKIP, SEEK, kReliable, kReadable, kSeekable);
_PW_CHANNEL(kDatagram, READ, SKIP, SKIP, kReliable, kReadable);

// _PW_CHANNEL(kDatagram, READ, WRTE, SEEK, kReadable, kWritable, kSeekable);
_PW_CHANNEL(kDatagram, READ, WRTE, SKIP, kReadable, kWritable);
// _PW_CHANNEL(kDatagram, READ, SKIP, SEEK, kReadable, kSeekable);
_PW_CHANNEL(kDatagram, READ, SKIP, SKIP, kReadable);

// _PW_CHANNEL(kDatagram, SKIP, WRTE, SEEK, kReliable, kWritable, kSeekable);
_PW_CHANNEL(kDatagram, SKIP, WRTE, SKIP, kReliable, kWritable);
// _PW_CHANNEL(kDatagram, SKIP, WRTE, SEEK, kWritable, kSeekable);
_PW_CHANNEL(kDatagram, SKIP, WRTE, SKIP, kWritable);

#undef _PW_CHANNEL
#undef _PW_CHANNEL_READABLE_READ
#undef _PW_CHANNEL_READABLE_SKIP
#undef _PW_CHANNEL_WRITABLE_WRTE
#undef _PW_CHANNEL_WRITABLE_SKIP
#undef _PW_CHANNEL_SEEKABLE_SEEK
#undef _PW_CHANNEL_SEEKABLE_SKIP
#undef _PW_CHANNEL_kDatagram
#undef _PW_CHANNEL_kByte

}  // namespace pw::channel
