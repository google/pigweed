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

#include <cstdint>
#include <numeric>
#include <vector>

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_containers/flat_map.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

// ########## Util functions

struct AclFrameWithStorage {
  std::vector<uint8_t> storage;
  emboss::AclDataFrameWriter writer;

  static constexpr size_t kH4HeaderSize = 1;
  pw::span<uint8_t> h4_span() { return storage; }
  pw::span<uint8_t> hci_span() {
    return pw::span(storage).subspan(kH4HeaderSize);
  }
};

// Allocate storage and populate an ACL packet header with the given length.
Result<AclFrameWithStorage> SetupAcl(uint16_t handle, uint16_t l2cap_length);

struct BFrameWithStorage {
  AclFrameWithStorage acl;
  emboss::BFrameWriter writer;
};

Result<BFrameWithStorage> SetupBFrame(uint16_t handle,
                                      uint16_t channel_id,
                                      uint16_t bframe_len);

struct CFrameWithStorage {
  AclFrameWithStorage acl;
  emboss::CFrameWriter writer;
};

Result<CFrameWithStorage> SetupCFrame(uint16_t handle,
                                      uint16_t channel_id,
                                      uint16_t cframe_len);

// Populate passed H4 command buffer and return Emboss view on it.
template <typename EmbossT>
Result<EmbossT> CreateAndPopulateToControllerView(H4PacketWithH4& h4_packet,
                                                  emboss::OpCode opcode,
                                                  size_t parameter_total_size) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 100);
  h4_packet.SetH4Type(emboss::H4PacketType::COMMAND);
  PW_TRY_ASSIGN(auto view, MakeEmbossWriter<EmbossT>(h4_packet.GetHciSpan()));
  view.header().opcode_enum().Write(opcode);
  view.header().parameter_total_size().Write(parameter_total_size);
  return view;
}

// Populate passed H4 event buffer and return Emboss view on it.
template <typename EmbossT>
Result<EmbossT> CreateAndPopulateToHostEventView(H4PacketWithHci& h4_packet,
                                                 emboss::EventCode event_code) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 0x10);
  h4_packet.SetH4Type(emboss::H4PacketType::EVENT);

  PW_TRY_ASSIGN(auto view, MakeEmbossWriter<EmbossT>(h4_packet.GetHciSpan()));
  view.header().event_code_enum().Write(event_code);
  view.status().Write(emboss::StatusCode::SUCCESS);
  EXPECT_TRUE(view.Ok());
  return view;
}

// Send an LE_Read_Buffer_Size (V2) CommandComplete event to `proxy` to request
// the reservation of a number of LE ACL send credits.
Status SendLeReadBufferResponseFromController(ProxyHost& proxy,
                                              uint8_t num_credits_to_reserve);

Status SendReadBufferResponseFromController(ProxyHost& proxy,
                                            uint8_t num_credits_to_reserve);

// Send a Number_of_Completed_Packets event to `proxy` that reports each
// {connection handle, number of completed packets} entry provided.
template <size_t kNumConnections>
Status SendNumberOfCompletedPackets(
    ProxyHost& proxy,
    containers::FlatMap<uint16_t, uint16_t, kNumConnections>
        packets_per_connection) {
  std::array<
      uint8_t,
      emboss::NumberOfCompletedPacketsEvent::MinSizeInBytes() +
          kNumConnections *
              emboss::NumberOfCompletedPacketsEventData::IntrinsicSizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci nocp_event{emboss::H4PacketType::EVENT, hci_arr};
  PW_TRY_ASSIGN(auto view,
                MakeEmbossWriter<emboss::NumberOfCompletedPacketsEventWriter>(
                    nocp_event.GetHciSpan()));
  view.header().event_code_enum().Write(
      emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);
  view.num_handles().Write(kNumConnections);

  size_t i = 0;
  for (const auto& [handle, num_packets] : packets_per_connection) {
    view.nocp_data()[i].connection_handle().Write(handle);
    view.nocp_data()[i].num_completed_packets().Write(num_packets);
    ++i;
  }

  proxy.HandleH4HciFromController(std::move(nocp_event));
  return OkStatus();
}

// Send a Connection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendConnectionCompleteEvent(ProxyHost& proxy,
                                   uint16_t handle,
                                   emboss::StatusCode status);

// Send a LE_Connection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendLeConnectionCompleteEvent(ProxyHost& proxy,
                                     uint16_t handle,
                                     emboss::StatusCode status);

// Send a Disconnection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendDisconnectionCompleteEvent(ProxyHost& proxy,
                                      uint16_t handle,
                                      bool successful = true);

Status SendL2capConnectionReq(ProxyHost& proxy,
                              uint16_t handle,
                              uint16_t source_cid,
                              uint16_t psm);

Status SendL2capConnectionRsp(ProxyHost& proxy,
                              uint16_t handle,
                              uint16_t source_cid,
                              uint16_t destination_cid,
                              emboss::L2capConnectionRspResultCode result_code);

Status SendL2capDisconnectRsp(ProxyHost& proxy,
                              uint16_t handle,
                              uint16_t source_cid,
                              uint16_t destination_cid);

// TODO: https://pwbug.dev/382783733 - Migrate to L2capChannelEvent callback.
struct CocParameters {
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  uint16_t remote_cid = 456;
  uint16_t rx_mtu = 100;
  uint16_t rx_mps = 100;
  uint16_t rx_credits = 1;
  uint16_t tx_mtu = 100;
  uint16_t tx_mps = 100;
  uint16_t tx_credits = 1;
  pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn = nullptr;
  pw::Function<void(L2capCoc::Event event)>&& event_fn = nullptr;
  pw::Function<void()>&& queue_space_available_fn = nullptr;
};

// Open and return an L2CAP connection-oriented channel managed by `proxy`.
L2capCoc BuildCoc(ProxyHost& proxy, CocParameters params);

struct RfcommParameters {
  uint16_t handle = 123;
  RfcommChannel::Config rx_config = {
      .cid = 234, .max_information_length = 900, .credits = 10};
  RfcommChannel::Config tx_config = {
      .cid = 456, .max_information_length = 900, .credits = 10};
  uint8_t rfcomm_channel = 3;
};

RfcommChannel BuildRfcomm(
    ProxyHost& proxy,
    RfcommParameters params = {},
    Function<void(pw::span<uint8_t> payload)>&& receive_fn = nullptr,
    Function<void()>&& queue_space_available_fn = nullptr,
    Function<void(L2capChannelEvent event)>&& event_fn = nullptr);

}  // namespace pw::bluetooth::proxy
