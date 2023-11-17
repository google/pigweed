// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"

#include <cinttypes>

namespace bt::l2cap {
namespace {
// Helper for implementing the AnyChannelMode equality operators.
template <typename T, typename... Alternatives>
bool VariantEqualsHelper(const std::variant<Alternatives...>& v, const T& x) {
  return std::holds_alternative<T>(v) && (std::get<T>(v) == x);
}

// Size for a string/buffer that ensures that pw::ToString<AnyChannelMode> will
// succeed.
constexpr size_t kAnyChannelModeMaxStringSize = 40;
}  // namespace

// Allow checking equality of AnyChannelMode with variant members.
bool operator==(const AnyChannelMode& any,
                RetransmissionAndFlowControlMode mode) {
  return VariantEqualsHelper(any, mode);
}
bool operator==(RetransmissionAndFlowControlMode mode,
                const AnyChannelMode& any) {
  return VariantEqualsHelper(any, mode);
}
bool operator==(const AnyChannelMode& any, CreditBasedFlowControlMode mode) {
  return VariantEqualsHelper(any, mode);
}
bool operator==(CreditBasedFlowControlMode mode, const AnyChannelMode& any) {
  return VariantEqualsHelper(any, mode);
}
bool operator!=(const AnyChannelMode& any,
                RetransmissionAndFlowControlMode mode) {
  return !VariantEqualsHelper(any, mode);
}
bool operator!=(RetransmissionAndFlowControlMode mode,
                const AnyChannelMode& any) {
  return !VariantEqualsHelper(any, mode);
}
bool operator!=(const AnyChannelMode& any, CreditBasedFlowControlMode mode) {
  return !VariantEqualsHelper(any, mode);
}
bool operator!=(CreditBasedFlowControlMode mode, const AnyChannelMode& any) {
  return !VariantEqualsHelper(any, mode);
}

std::string AnyChannelModeToString(const AnyChannelMode& mode) {
  std::string buffer(kAnyChannelModeMaxStringSize, 0);
  pw::StatusWithSize result = pw::ToString(mode, buffer);
  BT_ASSERT(result.ok());
  buffer.resize(result.size());
  return buffer;
}

pw::StatusWithSize AnyChannelModeToPwString(const AnyChannelMode& mode,
                                            pw::span<char> buffer) {
  return std::visit(
      [&](auto content) -> pw::StatusWithSize {
        if constexpr (std::is_same_v<decltype(content),
                                     RetransmissionAndFlowControlMode>) {
          static_assert(
              std::string_view("(RetransmissionAndFlowControlMode) 0x00")
                  .size() < kAnyChannelModeMaxStringSize);
          return pw::string::Format(
              buffer,
              "(RetransmissionAndFlowControlMode) %#.2" PRIx8,
              static_cast<uint8_t>(content));
        } else if constexpr (std::is_same_v<decltype(content),
                                            CreditBasedFlowControlMode>) {
          static_assert(
              std::string_view("(CreditBasedFlowControlMode) 0x00").size() <
              kAnyChannelModeMaxStringSize);
          return pw::string::Format(buffer,
                                    "(CreditBasedFlowControlMode) %#.2" PRIx8,
                                    static_cast<uint8_t>(content));
        }
      },
      mode);
}
}  // namespace bt::l2cap

namespace pw {
template <>
StatusWithSize ToString(const bt::l2cap::AnyChannelMode& mode,
                        span<char> buffer) {
  return bt::l2cap::AnyChannelModeToPwString(mode, buffer);
}
}  // namespace pw
