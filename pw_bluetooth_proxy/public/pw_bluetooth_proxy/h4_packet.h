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

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/emboss_util.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

/// H4PacketInterface is an abstract interface for an H4 HCI packet.
///
/// Concrete subclasses are used directly in code so their functions will be
/// properly inlined. This abstract superclass just ensures a common interface
/// across the concrete subclasses
class H4PacketInterface {
 public:
  H4PacketInterface() = default;

  H4PacketInterface(const H4PacketInterface& other) = delete;

  H4PacketInterface(H4PacketInterface&& other) = default;
  H4PacketInterface& operator=(H4PacketInterface&& other) = default;

  virtual ~H4PacketInterface() = default;

  /// Returns HCI packet type indicator as defined in BT Core Spec Version 5.4 |
  /// Vol 4, Part A, Section 2.
  virtual emboss::H4PacketType GetH4Type() = 0;

  /// Sets HCI packet type indicator.
  virtual void SetH4Type(emboss::H4PacketType) = 0;

  /// Returns pw::span of HCI packet as defined in BT Core Spec Version 5.4 |
  /// Vol 4, Part E, Section 5.4.
  virtual pw::span<uint8_t> GetHciSpan() = 0;

 protected:
  H4PacketInterface& operator=(const H4PacketInterface& other) = default;
};

/// H4PacketWithHci is an H4Packet backed by an HCI buffer.
class H4PacketWithHci final : public H4PacketInterface {
 public:
  H4PacketWithHci(emboss::H4PacketType h4_type, pw::span<uint8_t> hci_span)
      : hci_span_(hci_span), h4_type_(h4_type) {}

  H4PacketWithHci(const H4PacketWithHci& other) = delete;

  H4PacketWithHci(H4PacketWithHci&& other) = default;
  H4PacketWithHci& operator=(H4PacketWithHci&& other) = default;

  ~H4PacketWithHci() final = default;

  emboss::H4PacketType GetH4Type() final { return h4_type_; }

  void SetH4Type(emboss::H4PacketType h4_type) final { h4_type_ = h4_type; }

  pw::span<uint8_t> GetHciSpan() final { return hci_span_; }

 private:
  H4PacketWithHci& operator=(const H4PacketWithHci& other) = default;

  pw::span<uint8_t> hci_span_;

  emboss::H4PacketType h4_type_;
};

/// H4PacketWithHci is an H4Packet backed by an H4 buffer.
class H4PacketWithH4 final : public H4PacketInterface {
 public:
  H4PacketWithH4(pw::span<uint8_t> h4_span) : h4_span_(h4_span) {}

  H4PacketWithH4(emboss::H4PacketType h4_type, pw::span<uint8_t> h4_span)
      : H4PacketWithH4(h4_span) {
    SetH4Type(h4_type);
  }

  H4PacketWithH4(const H4PacketWithH4& other) = delete;

  H4PacketWithH4(H4PacketWithH4&& other) = default;
  H4PacketWithH4& operator=(H4PacketWithH4&& other) = default;

  ~H4PacketWithH4() final = default;

  emboss::H4PacketType GetH4Type() final {
    if (h4_span_.empty()) {
      return emboss::H4PacketType::UNKNOWN;
    }

    return emboss::H4PacketType(h4_span_[0]);
  }

  void SetH4Type(emboss::H4PacketType h4_type) final {
    if (!h4_span_.empty()) {
      h4_span_.data()[0] = cpp23::to_underlying(h4_type);
    }
  }

  pw::span<uint8_t> GetHciSpan() final {
    // If h4_span is empty, then return an empty span for hci also.
    if (h4_span_.empty()) {
      return {};
    }
    return H4HciSubspan(h4_span_);
  }

 private:
  H4PacketWithH4& operator=(const H4PacketWithH4& other) = default;

  pw::span<uint8_t> h4_span_;
};

}  // namespace pw::bluetooth::proxy
