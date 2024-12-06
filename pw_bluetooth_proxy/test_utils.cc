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

#include "pw_bluetooth_proxy_private/test_utils.h"

#include <cstdint>

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

// Allocate storage and populate an ACL packet header with the given length.
Result<AclFrameWithStorage> SetupAcl(uint16_t handle, uint16_t l2cap_length) {
  AclFrameWithStorage frame;

  frame.storage.resize(l2cap_length + emboss::AclDataFrame::MinSizeInBytes() +
                       AclFrameWithStorage::kH4HeaderSize);
  std::fill(frame.storage.begin(), frame.storage.end(), 0);
  PW_TRY_ASSIGN(frame.writer,
                MakeEmbossWriter<emboss::AclDataFrameWriter>(frame.hci_span()));
  frame.writer.header().handle().Write(handle);
  frame.writer.data_total_length().Write(l2cap_length);
  EXPECT_EQ(l2cap_length,
            frame.writer.payload().BackingStorage().SizeInBytes());
  return frame;
}

Result<BFrameWithStorage> SetupBFrame(uint16_t handle,
                                      uint16_t channel_id,
                                      uint16_t bframe_len) {
  BFrameWithStorage frame;
  PW_TRY_ASSIGN(
      frame.acl,
      SetupAcl(handle,
               bframe_len + emboss::BasicL2capHeader::IntrinsicSizeInBytes()));

  PW_TRY_ASSIGN(frame.writer,
                MakeEmbossWriter<emboss::BFrameWriter>(
                    frame.acl.writer.payload().BackingStorage().data(),
                    frame.acl.writer.payload().SizeInBytes()));
  frame.writer.pdu_length().Write(bframe_len);
  frame.writer.channel_id().Write(channel_id);
  EXPECT_TRUE(frame.writer.Ok());
  EXPECT_EQ(frame.writer.payload().SizeInBytes(), bframe_len);
  return frame;
}

Result<CFrameWithStorage> SetupCFrame(uint16_t handle,
                                      uint16_t channel_id,
                                      uint16_t cframe_len) {
  CFrameWithStorage frame;
  PW_TRY_ASSIGN(
      frame.acl,
      SetupAcl(handle,
               cframe_len + emboss::BasicL2capHeader::IntrinsicSizeInBytes()));

  PW_TRY_ASSIGN(frame.writer,
                MakeEmbossWriter<emboss::CFrameWriter>(
                    frame.acl.writer.payload().BackingStorage().data(),
                    frame.acl.writer.payload().SizeInBytes()));
  frame.writer.pdu_length().Write(cframe_len);
  frame.writer.channel_id().Write(channel_id);
  EXPECT_TRUE(frame.writer.Ok());
  EXPECT_EQ(frame.writer.payload().SizeInBytes(), cframe_len);
  return frame;
}

// Send an LE_Read_Buffer_Size (V2) CommandComplete event to `proxy` to request
// the reservation of a number of LE ACL send credits.
Status SendLeReadBufferResponseFromController(ProxyHost& proxy,
                                              uint8_t num_credits_to_reserve) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TRY_ASSIGN(auto view,
                CreateAndPopulateToHostEventView<
                    emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
                    h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V2);
  view.total_num_le_acl_data_packets().Write(num_credits_to_reserve);

  proxy.HandleH4HciFromController(std::move(h4_packet));
  return OkStatus();
}

Status SendReadBufferResponseFromController(ProxyHost& proxy,
                                            uint8_t num_credits_to_reserve) {
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TRY_ASSIGN(auto view,
                CreateAndPopulateToHostEventView<
                    emboss::ReadBufferSizeCommandCompleteEventWriter>(
                    h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view.total_num_acl_data_packets().Write(num_credits_to_reserve);
  EXPECT_TRUE(view.Ok());

  proxy.HandleH4HciFromController(std::move(h4_packet));
  return OkStatus();
}

// Send a Connection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendConnectionCompleteEvent(ProxyHost& proxy,
                                   uint16_t handle,
                                   emboss::StatusCode status) {
  std::array<uint8_t, emboss::ConnectionCompleteEvent::IntrinsicSizeInBytes()>
      hci_arr_dc{};
  H4PacketWithHci dc_event{emboss::H4PacketType::EVENT, hci_arr_dc};
  PW_TRY_ASSIGN(auto view,
                MakeEmbossWriter<emboss::ConnectionCompleteEventWriter>(
                    dc_event.GetHciSpan()));
  view.header().event_code_enum().Write(emboss::EventCode::CONNECTION_COMPLETE);
  view.status().Write(status);
  view.connection_handle().Write(handle);
  proxy.HandleH4HciFromController(std::move(dc_event));
  return OkStatus();
}

// Send a LE_Connection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendLeConnectionCompleteEvent(ProxyHost& proxy,
                                     uint16_t handle,
                                     emboss::StatusCode status) {
  std::array<uint8_t,
             emboss::LEConnectionCompleteSubevent::IntrinsicSizeInBytes()>
      hci_arr_dc{};
  H4PacketWithHci dc_event{emboss::H4PacketType::EVENT, hci_arr_dc};
  PW_TRY_ASSIGN(auto view,
                MakeEmbossWriter<emboss::LEConnectionCompleteSubeventWriter>(
                    dc_event.GetHciSpan()));
  view.le_meta_event().header().event_code_enum().Write(
      emboss::EventCode::LE_META_EVENT);
  view.le_meta_event().subevent_code_enum().Write(
      emboss::LeSubEventCode::CONNECTION_COMPLETE);
  view.status().Write(status);
  view.connection_handle().Write(handle);
  proxy.HandleH4HciFromController(std::move(dc_event));
  return OkStatus();
}

// Send a Disconnection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendDisconnectionCompleteEvent(ProxyHost& proxy,
                                      uint16_t handle,
                                      bool successful) {
  std::array<uint8_t,
             emboss::DisconnectionCompleteEvent::IntrinsicSizeInBytes()>
      hci_arr_dc;
  hci_arr_dc.fill(0);
  H4PacketWithHci dc_event{emboss::H4PacketType::EVENT, hci_arr_dc};
  PW_TRY_ASSIGN(auto view,
                MakeEmbossWriter<emboss::DisconnectionCompleteEventWriter>(
                    dc_event.GetHciSpan()));
  view.header().event_code_enum().Write(
      emboss::EventCode::DISCONNECTION_COMPLETE);
  view.status().Write(successful ? emboss::StatusCode::SUCCESS
                                 : emboss::StatusCode::HARDWARE_FAILURE);
  view.connection_handle().Write(handle);
  proxy.HandleH4HciFromController(std::move(dc_event));
  return OkStatus();
}

Status SendL2capConnectionReq(ProxyHost& proxy,
                              uint16_t handle,
                              uint16_t source_cid,
                              uint16_t psm) {
  // First send CONNECTION_REQ to setup partial connection.
  constexpr size_t kConnectionReqLen =
      emboss::L2capConnectionReq::IntrinsicSizeInBytes();
  PW_TRY_ASSIGN(
      CFrameWithStorage cframe,
      SetupCFrame(handle,
                  cpp23::to_underlying(emboss::L2capFixedCid::ACL_U_SIGNALING),
                  kConnectionReqLen));

  emboss::L2capConnectionReqWriter conn_req_writer =
      emboss::MakeL2capConnectionReqView(
          cframe.writer.payload().BackingStorage().data(),
          cframe.writer.payload().SizeInBytes());
  conn_req_writer.command_header().code().Write(
      emboss::L2capSignalingPacketCode::CONNECTION_REQ);
  // Note data_length doesn't include command header.
  conn_req_writer.command_header().data_length().Write(
      kConnectionReqLen -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  conn_req_writer.psm().Write(psm);
  conn_req_writer.source_cid().Write(source_cid);

  H4PacketWithHci connection_req_packet{emboss::H4PacketType::ACL_DATA,
                                        cframe.acl.hci_span()};
  proxy.HandleH4HciFromController(std::move(connection_req_packet));
  return OkStatus();
}

Status SendL2capConnectionRsp(
    ProxyHost& proxy,
    uint16_t handle,
    uint16_t source_cid,
    uint16_t destination_cid,
    emboss::L2capConnectionRspResultCode result_code) {
  constexpr size_t kConnectionRspLen =
      emboss::L2capConnectionRsp::MinSizeInBytes();
  PW_TRY_ASSIGN(
      auto cframe,
      SetupCFrame(handle,
                  cpp23::to_underlying(emboss::L2capFixedCid::ACL_U_SIGNALING),
                  kConnectionRspLen));

  emboss::L2capConnectionRspWriter conn_rsp_writer =
      emboss::MakeL2capConnectionRspView(
          cframe.writer.payload().BackingStorage().data(),
          cframe.writer.payload().SizeInBytes());
  conn_rsp_writer.command_header().code().Write(
      emboss::L2capSignalingPacketCode::CONNECTION_RSP);

  conn_rsp_writer.command_header().data_length().Write(
      kConnectionRspLen -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  conn_rsp_writer.source_cid().Write(source_cid);
  conn_rsp_writer.destination_cid().Write(destination_cid);
  conn_rsp_writer.result().Write(result_code);

  H4PacketWithH4 connection_rsp_packet{emboss::H4PacketType::ACL_DATA,
                                       cframe.acl.h4_span()};
  proxy.HandleH4HciFromHost(std::move(connection_rsp_packet));
  return OkStatus();
}

Status SendL2capDisconnectRsp(ProxyHost& proxy,
                              uint16_t handle,
                              uint16_t source_cid,
                              uint16_t destination_cid) {
  constexpr size_t kDisconnectionRspLen =
      emboss::L2capDisconnectionRsp::MinSizeInBytes();
  PW_TRY_ASSIGN(
      auto cframe,
      SetupCFrame(handle,
                  cpp23::to_underlying(emboss::L2capFixedCid::ACL_U_SIGNALING),
                  kDisconnectionRspLen));

  emboss::L2capDisconnectionRspWriter disconn_rsp_writer =
      emboss::MakeL2capDisconnectionRspView(
          cframe.writer.payload().BackingStorage().data(),
          cframe.writer.payload().SizeInBytes());
  disconn_rsp_writer.command_header().code().Write(
      emboss::L2capSignalingPacketCode::DISCONNECTION_RSP);

  disconn_rsp_writer.command_header().data_length().Write(
      kDisconnectionRspLen -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  disconn_rsp_writer.source_cid().Write(source_cid);
  disconn_rsp_writer.destination_cid().Write(destination_cid);

  H4PacketWithH4 packet{emboss::H4PacketType::ACL_DATA, cframe.acl.h4_span()};
  proxy.HandleH4HciFromHost(std::move(packet));
  return OkStatus();
}

// Open and return an L2CAP connection-oriented channel managed by `proxy`.
L2capCoc BuildCoc(ProxyHost& proxy, CocParameters params) {
  pw::Result<L2capCoc> channel =
      proxy.AcquireL2capCoc(params.handle,
                            {.cid = params.local_cid,
                             .mtu = params.rx_mtu,
                             .mps = params.rx_mps,
                             .credits = params.rx_credits},
                            {.cid = params.remote_cid,
                             .mtu = params.tx_mtu,
                             .mps = params.tx_mps,
                             .credits = params.tx_credits},
                            std::move(params.receive_fn),
                            std::move(params.event_fn),
                            std::move(params.queue_space_available_fn));
  return std::move(channel.value());
}

RfcommChannel BuildRfcomm(
    ProxyHost& proxy,
    RfcommParameters params,
    pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn,
    pw::Function<void()>&& queue_space_available_fn) {
  pw::Result<RfcommChannel> channel =
      proxy.AcquireRfcommChannel(params.handle,
                                 params.rx_config,
                                 params.tx_config,
                                 params.rfcomm_channel,
                                 std::move(receive_fn),
                                 std::move(queue_space_available_fn));
  PW_TEST_EXPECT_OK(channel);
  return std::move((channel.value()));
}

}  // namespace pw::bluetooth::proxy
