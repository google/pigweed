#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"

namespace bt::sco {

// Codec SCO parameter sets as defined in HFP specification (v1.8, section 5.7).
struct ParameterSet {
  uint16_t packet_types;
  uint32_t transmit_receive_bandwidth;
  hci::CodingFormat transmit_receive_format;
  uint16_t max_latency_ms;
  hci::ScoRetransmissionEffort retransmission_effort;
};

constexpr ParameterSet kParameterSetMsbcT1{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(hci::ScoPacketTypeBits::kNot2Ev3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kTransparent,
    .max_latency_ms = 8,
    .retransmission_effort = hci::ScoRetransmissionEffort::kQualityOptimized};

constexpr ParameterSet kParameterSetMsbcT2{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kEv3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kTransparent,
    .max_latency_ms = 13,
    .retransmission_effort = hci::ScoRetransmissionEffort::kQualityOptimized};

constexpr ParameterSet kParameterSetCvsdS1{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kEv3) |
                    static_cast<uint8_t>(hci::ScoPacketTypeBits::kNot2Ev3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kCvsd,
    .max_latency_ms = 7,
    .retransmission_effort = hci::ScoRetransmissionEffort::kPowerOptimized};

constexpr ParameterSet kParameterSetCvsdS2{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kEv3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kCvsd,
    .max_latency_ms = 7,
    .retransmission_effort = hci::ScoRetransmissionEffort::kPowerOptimized};

constexpr ParameterSet kParameterSetCvsdS3{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kEv3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kCvsd,
    .max_latency_ms = 10,
    .retransmission_effort = hci::ScoRetransmissionEffort::kPowerOptimized};

constexpr ParameterSet kParameterSetCvsdS4{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kEv3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kCvsd,
    .max_latency_ms = 12,
    .retransmission_effort = hci::ScoRetransmissionEffort::kQualityOptimized};

constexpr ParameterSet kParameterSetCvsdD0{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kHv1),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kCvsd,
    .max_latency_ms = 0xFFFF,
    .retransmission_effort = hci::ScoRetransmissionEffort::kDontCare};

constexpr ParameterSet kParameterSetCvsdD1{
    .packet_types = static_cast<uint8_t>(hci::ScoPacketTypeBits::kHv3),
    .transmit_receive_bandwidth = 8000,
    .transmit_receive_format = hci::CodingFormat::kCvsd,
    .max_latency_ms = 0xFFFF,
    .retransmission_effort = hci::ScoRetransmissionEffort::kDontCare};

}  // namespace bt::sco

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_H_
