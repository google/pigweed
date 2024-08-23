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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream.h"

#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"

namespace bt::iso {

class IsoStreamImpl final : public IsoStream {
 public:
  IsoStreamImpl(uint8_t cig_id,
                uint8_t cis_id,
                hci_spec::ConnectionHandle cis_handle,
                CisEstablishedCallback on_established_cb,
                hci::CommandChannel::WeakPtr cmd,
                pw::Callback<void()> on_closed_cb);

  // IsoStream overrides
  bool OnCisEstablished(const hci::EmbossEventPacket& event) override;
  void SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection direction,
      const bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>& codec_id,
      const std::optional<std::vector<uint8_t>>& codec_configuration,
      uint32_t controller_delay_usecs,
      fit::function<void(SetupDataPathError)> cb) override;
  hci_spec::ConnectionHandle cis_handle() const override {
    return cis_hci_handle_;
  }
  void Close() override;
  IsoStream::WeakPtr GetWeakPtr() override { return weak_self_.GetWeakPtr(); }

  // IsoDataChannel::ConnectionInterface override
  void ReceiveInboundPacket() override;

 private:
  enum class IsoStreamState {
    kNotEstablished,
    kEstablished,
  } state_;

  uint8_t cig_id_ __attribute__((unused));
  uint8_t cis_id_ __attribute__((unused));

  // Connection parameters, only valid after CIS is established
  CisEstablishedParameters cis_params_;

  // Handle assigned by the controller
  hci_spec::ConnectionHandle cis_hci_handle_;

  // Called after HCI_LE_CIS_Established event is received and handled
  CisEstablishedCallback cis_established_cb_;

  // Called when stream is closed
  pw::Callback<void()> on_closed_cb_;

  // Has the data path been configured?
  enum class DataPathState {
    kNotSetUp,
    kSettingUp,
    kSetUp,
  };
  DataPathState input_data_path_state_ = DataPathState::kNotSetUp;
  DataPathState output_data_path_state_ = DataPathState::kNotSetUp;

  hci::CommandChannel::WeakPtr cmd_;

  hci::CommandChannel::EventHandlerId cis_established_handler_;

  WeakSelf<IsoStreamImpl> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(IsoStreamImpl);
};

IsoStreamImpl::IsoStreamImpl(uint8_t cig_id,
                             uint8_t cis_id,
                             hci_spec::ConnectionHandle cis_handle,
                             CisEstablishedCallback on_established_cb,
                             hci::CommandChannel::WeakPtr cmd,
                             pw::Callback<void()> on_closed_cb)
    : IsoStream(),
      state_(IsoStreamState::kNotEstablished),
      cig_id_(cig_id),
      cis_id_(cis_id),
      cis_hci_handle_(cis_handle),
      cis_established_cb_(std::move(on_established_cb)),
      on_closed_cb_(std::move(on_closed_cb)),
      cmd_(std::move(cmd)),
      weak_self_(this) {
  BT_ASSERT(cmd_.is_alive());

  auto self = weak_self_.GetWeakPtr();
  cis_established_handler_ = cmd_->AddLEMetaEventHandler(
      hci_spec::kLECISEstablishedSubeventCode,
      [self = std::move(self)](const hci::EmbossEventPacket& event) {
        if (!self.is_alive()) {
          return hci::CommandChannel::EventCallbackResult::kRemove;
        }
        if (self->OnCisEstablished(event)) {
          self->cis_established_handler_ = 0u;
          return hci::CommandChannel::EventCallbackResult::kRemove;
        }
        return hci::CommandChannel::EventCallbackResult::kContinue;
      });
  BT_ASSERT(cis_established_handler_ != 0u);
}

bool IsoStreamImpl::OnCisEstablished(const hci::EmbossEventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_ASSERT(event.view<pw::bluetooth::emboss::LEMetaEventView>()
                .subevent_code()
                .Read() == hci_spec::kLECISEstablishedSubeventCode);
  auto view = event.view<pw::bluetooth::emboss::LECISEstablishedSubeventView>();

  // Ignore any events intended for another CIS
  hci_spec::ConnectionHandle handle = view.connection_handle().Read();
  if (handle != cis_hci_handle_) {
    bt_log(
        INFO,
        "iso",
        "Ignoring CIS established notification for handle 0x%x (target: 0x%x)",
        handle,
        cis_hci_handle_);

    // Event not handled
    return false;
  }

  pw::bluetooth::emboss::StatusCode status = view.status().Read();
  bt_log(INFO,
         "iso",
         "Handling CIS established notification for handle 0x%x (status: %s)",
         handle,
         hci_spec::StatusCodeToString(status).c_str());

  if (status != pw::bluetooth::emboss::StatusCode::SUCCESS) {
    cis_established_cb_(status, std::nullopt, std::nullopt);
    Close();
    return true;
  }

  state_ = IsoStreamState::kEstablished;

  // General stream attributes
  cis_params_.cig_sync_delay = view.cig_sync_delay().Read();
  cis_params_.cis_sync_delay = view.cis_sync_delay().Read();
  cis_params_.max_subevents = view.nse().Read();
  cis_params_.iso_interval = view.iso_interval().Read();

  // Central => Peripheral stream attributes
  CisEstablishedParameters::CisUnidirectionalParams* params =
      &cis_params_.c_to_p_params;
  params->transport_latency = view.transport_latency_c_to_p().Read();
  params->phy = view.phy_c_to_p().Read();
  params->burst_number = view.bn_c_to_p().Read();
  params->flush_timeout = view.ft_c_to_p().Read();
  params->max_pdu_size = view.max_pdu_c_to_p().Read();

  // Peripheral => Central stream attributes
  params = &cis_params_.p_to_c_params;
  params->transport_latency = view.transport_latency_p_to_c().Read();
  params->phy = view.phy_p_to_c().Read();
  params->burst_number = view.bn_p_to_c().Read();
  params->flush_timeout = view.ft_p_to_c().Read();
  params->max_pdu_size = view.max_pdu_p_to_c().Read();

  cis_established_cb_(status, GetWeakPtr(), cis_params_);

  // Event handled
  return true;
}

void IsoStreamImpl::SetupDataPath(
    pw::bluetooth::emboss::DataPathDirection direction,
    const bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>& codec_id,
    const std::optional<std::vector<uint8_t>>& codec_configuration,
    uint32_t controller_delay_usecs,
    fit::function<void(IsoStream::SetupDataPathError)> cb) {
  if (state_ != IsoStreamState::kEstablished) {
    bt_log(WARN, "iso", "failed to setup data path - CIS not established");
    cb(kCisNotEstablished);
    return;
  }

  DataPathState* target_data_path_state;
  const char* direction_as_str;
  switch (direction) {
    case pw::bluetooth::emboss::DataPathDirection::INPUT:
      target_data_path_state = &input_data_path_state_;
      direction_as_str = "Input";
      break;
    case pw::bluetooth::emboss::DataPathDirection::OUTPUT:
      target_data_path_state = &output_data_path_state_;
      direction_as_str = "Output";
      break;
    default:
      bt_log(WARN,
             "iso",
             "invalid data path direction (%u)",
             static_cast<unsigned>(direction));
      cb(kInvalidArgs);
      return;
  }

  if (*target_data_path_state != DataPathState::kNotSetUp) {
    bt_log(WARN,
           "iso",
           "attempt to setup %s CIS path - already setup",
           direction_as_str);
    cb(kStreamAlreadyExists);
    return;
  }

  bt_log(INFO, "iso", "setting up CIS data path for %s", direction_as_str);
  size_t packet_size =
      pw::bluetooth::emboss::LESetupISODataPathCommand::MinSizeInBytes() +
      (codec_configuration.has_value() ? codec_configuration->size() : 0);
  auto cmd_packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetupISODataPathCommandWriter>(
      hci_spec::kLESetupISODataPath, packet_size);
  auto cmd_view = cmd_packet.view_t();
  cmd_view.connection_handle().Write(cis_hci_handle_);
  cmd_view.data_path_direction().Write(direction);
  cmd_view.data_path_id().Write(0);
  cmd_view.codec_id().CopyFrom(
      const_cast<bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>&>(
          codec_id)
          .view());
  cmd_view.controller_delay().Write(controller_delay_usecs);
  if (codec_configuration.has_value()) {
    cmd_view.codec_configuration_length().Write(codec_configuration->size());
    std::memcpy(cmd_view.codec_configuration().BackingStorage().data(),
                codec_configuration->data(),
                codec_configuration->size());
  } else {
    cmd_view.codec_configuration_length().Write(0);
  }

  *target_data_path_state = DataPathState::kSettingUp;
  auto self = GetWeakPtr();

  bt_log(INFO, "iso", "sending LE_Setup_ISO_Data_Path command");
  cmd_->SendCommand(
      std::move(cmd_packet),
      [cb = std::move(cb),
       self,
       cis_handle = cis_hci_handle_,
       target_data_path_state](auto id,
                               const hci::EmbossEventPacket& cmd_complete) {
        if (!self.is_alive()) {
          cb(kStreamClosed);
          return;
        }

        auto return_params =
            cmd_complete.view<pw::bluetooth::emboss::
                                  LESetupISODataPathCommandCompleteEventView>();
        pw::bluetooth::emboss::StatusCode status =
            return_params.status().Read();
        hci_spec::ConnectionHandle connection_handle =
            return_params.connection_handle().Read();

        if (status != pw::bluetooth::emboss::StatusCode::SUCCESS) {
          bt_log(ERROR,
                 "iso",
                 "failed to setup ISO data path for handle 0x%x (status: 0x%x)",
                 connection_handle,
                 static_cast<uint8_t>(status));
          *target_data_path_state = DataPathState::kNotSetUp;
          cb(kStreamRejectedByController);
          return;
        }

        // It's hard to know what is the right thing to do here. The controller
        // accepted our request, but we don't agree on the connection handle ID.
        // Something is amiss, so we will refuse to consider the data path
        // setup even though the controller may think otherwise.
        if (connection_handle != cis_handle) {
          bt_log(ERROR,
                 "iso",
                 "handle mismatch in ISO data path setup completion (expected: "
                 "0x%x, actual: %x)",
                 cis_handle,
                 connection_handle);
          *target_data_path_state = DataPathState::kNotSetUp;
          cb(kStreamRejectedByController);
          return;
        }

        bt_log(INFO, "iso", "successfully set up data path");
        *target_data_path_state = DataPathState::kSetUp;
        cb(kSuccess);
      });
}

void IsoStreamImpl::ReceiveInboundPacket() {
  // TODO(fxbug.dev/311639690): implement
}

void IsoStreamImpl::Close() { on_closed_cb_(); }

std::unique_ptr<IsoStream> IsoStream::Create(
    uint8_t cig_id,
    uint8_t cis_id,
    hci_spec::ConnectionHandle cis_handle,
    CisEstablishedCallback on_established_cb,
    hci::CommandChannel::WeakPtr cmd,
    pw::Callback<void()> on_closed_cb) {
  return std::make_unique<IsoStreamImpl>(cig_id,
                                         cis_id,
                                         cis_handle,
                                         std::move(on_established_cb),
                                         std::move(cmd),
                                         std::move(on_closed_cb));
}

}  // namespace bt::iso
