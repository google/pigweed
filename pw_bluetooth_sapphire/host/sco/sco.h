/*
 * Copyright 2020 The Fuchsia Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_H_

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

#include <pw_bluetooth/hci.emb.h>

namespace bt::sco {

// TODO(fxbug.dev/110686): This type should be deleted as it duplicates `ScoPacketType` in
// hci-protocol.emb. This may involve migrating `ParameterSet` below to Emboss.
enum class ScoPacketTypeBits : uint16_t {
  // SCO packet types
  kHv1 = (1 << 0),
  kHv2 = (1 << 1),
  kHv3 = (1 << 2),
  // eSCO packet types
  kEv3 = (1 << 3),
  kEv4 = (1 << 4),
  kEv5 = (1 << 5),
  kNot2Ev3 = (1 << 6),
  kNot3Ev3 = (1 << 7),
  kNot2Ev5 = (1 << 8),
  kNot3Ev5 = (1 << 9),
};

// Codec SCO parameter sets as defined in HFP specification (v1.8, section 5.7).
struct ParameterSet {
  uint16_t packet_types;
  uint32_t transmit_receive_bandwidth;
  pw::bluetooth::emboss::CodingFormat transmit_receive_format;
  uint16_t max_latency_ms;
  hci_spec::ScoRetransmissionEffort retransmission_effort;
};

constexpr ParameterSet kParameterSetMsbcT1{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot2Ev5) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot3Ev5),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::TRANSPARENT,
    .max_latency_ms = 8,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kQualityOptimized};

constexpr ParameterSet kParameterSetMsbcT2{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot2Ev5) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot3Ev5),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::TRANSPARENT,
    .max_latency_ms = 13,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kQualityOptimized};

constexpr ParameterSet kParameterSetCvsdS1{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kHv1) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kHv2) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kHv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot2Ev5) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot3Ev5),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::CVSD,
    .max_latency_ms = 7,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kPowerOptimized};

constexpr ParameterSet kParameterSetCvsdS2{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot2Ev5) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot3Ev5),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::CVSD,
    .max_latency_ms = 7,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kPowerOptimized};

constexpr ParameterSet kParameterSetCvsdS3{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot2Ev5) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot3Ev5),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::CVSD,
    .max_latency_ms = 10,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kPowerOptimized};

constexpr ParameterSet kParameterSetCvsdS4{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot2Ev5) |
                    static_cast<uint8_t>(ScoPacketTypeBits::kNot3Ev5),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::CVSD,
    .max_latency_ms = 12,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kQualityOptimized};

constexpr ParameterSet kParameterSetCvsdD0{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kHv1),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::CVSD,
    .max_latency_ms = 0xFFFF,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kDontCare};

constexpr ParameterSet kParameterSetCvsdD1{
    .packet_types = static_cast<uint8_t>(ScoPacketTypeBits::kHv3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = pw::bluetooth::emboss::CodingFormat::CVSD,
    .max_latency_ms = 0xFFFF,
    .retransmission_effort = hci_spec::ScoRetransmissionEffort::kDontCare};

}  // namespace bt::sco

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_H_
