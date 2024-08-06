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

namespace bt::iso {

class IsoStreamImpl final : public IsoStream {
 public:
  IsoStreamImpl(uint8_t cig_id,
                uint8_t cis_id,
                hci_spec::ConnectionHandle cis_handle,
                CisEstablishedCallback on_established_cb,
                hci::CommandChannel::WeakPtr cmd_channel,
                pw::Callback<void()> on_closed_cb);

  bool OnCisEstablished(const hci::EmbossEventPacket& event) override;

  void SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection direction,
      const bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>& codec_id,
      std::optional<std::vector<uint8_t>> codec_configuration,
      uint32_t controller_delay_usecs,
      fit::function<void(SetupDataPathError)> cb) override;

  void Close() override;

  IsoStream::WeakPtr GetWeakPtr() override { return weak_self_.GetWeakPtr(); }

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

  hci::CommandChannel::WeakPtr cmd_;

  hci::CommandChannel::EventHandlerId cis_established_handler_;

  WeakSelf<IsoStreamImpl> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(IsoStreamImpl);
};

IsoStreamImpl::IsoStreamImpl(uint8_t cig_id,
                             uint8_t cis_id,
                             hci_spec::ConnectionHandle cis_handle,
                             CisEstablishedCallback on_established_cb,
                             hci::CommandChannel::WeakPtr cmd_channel,
                             pw::Callback<void()> on_closed_cb)
    : IsoStream(),
      state_(IsoStreamState::kNotEstablished),
      cig_id_(cig_id),
      cis_id_(cis_id),
      cis_hci_handle_(cis_handle),
      cis_established_cb_(std::move(on_established_cb)),
      on_closed_cb_(std::move(on_closed_cb)),
      cmd_(cmd_channel),
      weak_self_(this) {
  auto self = weak_self_.GetWeakPtr();

  if (!cmd_.is_alive()) {
    return;
  }

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
    std::optional<std::vector<uint8_t>> codec_configuration,
    uint32_t controller_delay_usecs,
    fit::function<void(IsoStream::SetupDataPathError)> cb) {
  // TODO(fxbug.dev/311639690): implement
  cb(kSuccess);
}

void IsoStreamImpl::Close() { on_closed_cb_(); }

std::unique_ptr<IsoStream> IsoStream::Create(
    uint8_t cig_id,
    uint8_t cis_id,
    hci_spec::ConnectionHandle cis_handle,
    CisEstablishedCallback on_established_cb,
    hci::CommandChannel::WeakPtr cmd_channel,
    pw::Callback<void()> on_closed_cb) {
  return std::make_unique<IsoStreamImpl>(cig_id,
                                         cis_id,
                                         cis_handle,
                                         std::move(on_established_cb),
                                         std::move(cmd_channel),
                                         std::move(on_closed_cb));
}

}  // namespace bt::iso
