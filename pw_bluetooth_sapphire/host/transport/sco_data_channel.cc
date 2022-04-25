// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_data_channel.h"

#include <lib/async/default.h>
#include <lib/fit/defer.h>
#include <zircon/status.h>

#include "device_wrapper.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::hci {
class ScoDataChannelImpl final : public ScoDataChannel {
 public:
  ScoDataChannelImpl(const DataBufferInfo& buffer_info, CommandChannel* command_channel,
                     HciWrapper* hci);
  ~ScoDataChannelImpl() override;

  // ScoDataChannel overrides:
  void RegisterConnection(fxl::WeakPtr<ConnectionInterface> connection) override;
  void UnregisterConnection(hci_spec::ConnectionHandle handle) override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override;
  void OnOutboundPacketReadable() override;
  uint16_t max_data_length() const override { return buffer_info_.max_data_length(); }

 private:
  enum class HciConfigState {
    kPending,
    kConfigured,
  };

  struct ConnectionData {
    fxl::WeakPtr<ConnectionInterface> connection;
    HciConfigState config_state = HciConfigState::kPending;
  };

  void OnRxPacket(std::unique_ptr<ScoDataPacket> packet);

  // Send packets queued on the active channel if the controller has free buffer slots.
  void TrySendNextPackets();

  // The number of free buffer slots in the controller.
  size_t GetNumFreePackets();

  // Chooses a new connection to be active if one isn't already active.
  void MaybeUpdateActiveConnection();

  // Configure the HCI driver for the active SCO connection. Must be called before sending packets.
  // Called when the active connection is changed.
  void ConfigureHci();

  // Called when SCO configuration is complete.
  void OnHciConfigured(hci_spec::ConnectionHandle conn_handle, zx_status_t status);

  // Handler for the HCI Number of Completed Packets Event, used for
  // packet-based data flow control.
  CommandChannel::EventCallbackResult OnNumberOfCompletedPacketsEvent(const EventPacket& event);

  bool IsActiveConnectionConfigured() {
    if (!active_connection_) {
      return false;
    }
    auto iter = connections_.find(active_connection_->handle());
    ZX_ASSERT(iter != connections_.end());
    return (iter->second.config_state == HciConfigState::kConfigured);
  }

  CommandChannel* command_channel_;
  HciWrapper* hci_;
  DataBufferInfo buffer_info_;

  async_dispatcher_t* dispatcher_;

  std::unordered_map<hci_spec::ConnectionHandle, ConnectionData> connections_;

  // Only 1 connection may send packets at a time.
  fxl::WeakPtr<ConnectionInterface> active_connection_;

  // Stores per-connection counts of unacknowledged packets sent to the controller. Entries are
  // updated/removed on the HCI Number Of Completed Packets event and removed when a connection is
  // unregistered (the controller does not acknowledge packets of disconnected links).
  std::unordered_map<hci_spec::ConnectionHandle, size_t> pending_packet_counts_;

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_;

  fxl::WeakPtrFactory<ScoDataChannelImpl> weak_ptr_factory_{this};
};

ScoDataChannelImpl::ScoDataChannelImpl(const DataBufferInfo& buffer_info,
                                       CommandChannel* command_channel, HciWrapper* hci)
    : command_channel_(command_channel),
      hci_(hci),
      buffer_info_(buffer_info),
      dispatcher_(async_get_default_dispatcher()) {
  // ScoDataChannel shouldn't be used if the buffer is unavailable (implying the controller
  // doesn't support SCO).
  ZX_ASSERT(buffer_info_.IsAvailable());

  num_completed_packets_event_handler_id_ = command_channel_->AddEventHandler(
      hci_spec::kNumberOfCompletedPacketsEventCode,
      fit::bind_member<&ScoDataChannelImpl::OnNumberOfCompletedPacketsEvent>(this));
  ZX_ASSERT(num_completed_packets_event_handler_id_);

  hci_->SetScoCallback(fit::bind_member<&ScoDataChannelImpl::OnRxPacket>(this));
}

ScoDataChannelImpl::~ScoDataChannelImpl() {
  command_channel_->RemoveEventHandler(num_completed_packets_event_handler_id_);
}

void ScoDataChannelImpl::RegisterConnection(fxl::WeakPtr<ConnectionInterface> connection) {
  ZX_ASSERT(connection->parameters().output_data_path == hci_spec::ScoDataPath::kHci);
  ConnectionData conn_data{.connection = connection};
  auto [_, inserted] = connections_.emplace(connection->handle(), std::move(conn_data));
  ZX_ASSERT_MSG(inserted, "connection with handle %#.4x already registered", connection->handle());
  MaybeUpdateActiveConnection();
}

void ScoDataChannelImpl::UnregisterConnection(hci_spec::ConnectionHandle handle) {
  auto iter = connections_.find(handle);
  if (iter == connections_.end()) {
    return;
  }
  connections_.erase(iter);
  MaybeUpdateActiveConnection();
}

void ScoDataChannelImpl::ClearControllerPacketCount(hci_spec::ConnectionHandle handle) {
  bt_log(DEBUG, "hci", "clearing pending packets (handle: %#.4x)", handle);
  ZX_ASSERT(connections_.find(handle) == connections_.end());

  auto iter = pending_packet_counts_.find(handle);
  if (iter == pending_packet_counts_.end()) {
    return;
  }

  pending_packet_counts_.erase(iter);
  TrySendNextPackets();
}

void ScoDataChannelImpl::OnOutboundPacketReadable() { TrySendNextPackets(); }

void ScoDataChannelImpl::OnRxPacket(std::unique_ptr<ScoDataPacket> packet) {
  auto conn_iter = connections_.find(letoh16(packet->connection_handle()));
  if (conn_iter == connections_.end()) {
    // Ignore inbound packets for connections that aren't registered. Unlike ACL, buffering data
    // received before a connection is registered is unnecessary for SCO (it's realtime and
    // not expected to be reliable).
    bt_log(DEBUG, "hci", "ignoring inbound SCO packet for unregistered connection: %#.4x",
           packet->connection_handle());
    return;
  }
  conn_iter->second.connection->ReceiveInboundPacket(std::move(packet));
}

CommandChannel::EventCallbackResult ScoDataChannelImpl::OnNumberOfCompletedPacketsEvent(
    const EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci_spec::kNumberOfCompletedPacketsEventCode);
  const auto& payload = event.params<hci_spec::NumberOfCompletedPacketsEventParams>();

  const size_t handles_in_packet =
      (event.view().payload_size() - sizeof(hci_spec::NumberOfCompletedPacketsEventParams)) /
      sizeof(hci_spec::NumberOfCompletedPacketsEventData);

  if (payload.number_of_handles != handles_in_packet) {
    bt_log(ERROR, "hci",
           "packets handle count (%d) doesn't match params size (%zu); either the packet was "
           "parsed incorrectly or the controller is buggy",
           payload.number_of_handles, handles_in_packet);
  }

  for (uint8_t i = 0; i < payload.number_of_handles && i < handles_in_packet; ++i) {
    const hci_spec::NumberOfCompletedPacketsEventData* data = payload.data + i;

    auto iter = pending_packet_counts_.find(le16toh(data->connection_handle));
    if (iter == pending_packet_counts_.end()) {
      // This is expected if the completed packet is an ACL packet.
      bt_log(TRACE, "hci",
             "controller reported completed packets for connection handle without pending packets: "
             "%#.4x",
             data->connection_handle);
      continue;
    }

    const uint16_t comp_packets = le16toh(data->hc_num_of_completed_packets);

    ZX_ASSERT(iter->second != 0u);
    ZX_ASSERT_MSG(
        iter->second >= comp_packets,
        "pending/completed packet count mismatch! (handle: %#.4x, pending: %zu, completed : %u)",
        le16toh(data->connection_handle), iter->second, comp_packets);
    iter->second -= comp_packets;
    if (iter->second == 0u) {
      pending_packet_counts_.erase(iter);
    }
  }

  TrySendNextPackets();
  return CommandChannel::EventCallbackResult::kContinue;
}

void ScoDataChannelImpl::TrySendNextPackets() {
  if (!IsActiveConnectionConfigured()) {
    // If there is no active connection configured, then there is probably no bandwidth, so we
    // shouldn't send packets.
    return;
  }

  // Even though we only expect to have enough bandwidth for the 1 active/configured SCO connection
  // (especially for USB, see fxb/91560), try to service all connections.
  for (auto& [conn_handle, conn_data] : connections_) {
    for (size_t num_free_packets = GetNumFreePackets(); num_free_packets != 0u;
         num_free_packets--) {
      std::unique_ptr<ScoDataPacket> packet = conn_data.connection->GetNextOutboundPacket();
      if (!packet) {
        // This connection has no more packets available.
        break;
      }

      zx_status_t status = hci_->SendScoPacket(std::move(packet));
      if (status != ZX_OK) {
        bt_log(ERROR, "hci", "failed to send data packet to HCI driver (%s) - dropping packet",
               zx_status_get_string(status));
        continue;
      }

      auto [iter, _] = pending_packet_counts_.try_emplace(conn_handle, 0u);
      iter->second++;
    }
  }
}

size_t ScoDataChannelImpl::GetNumFreePackets() {
  size_t pending_packets_sum = 0u;
  for (auto& [_, count] : pending_packet_counts_) {
    pending_packets_sum += count;
  }
  return buffer_info_.max_num_packets() - pending_packets_sum;
}

void ScoDataChannelImpl::MaybeUpdateActiveConnection() {
  if (active_connection_ && connections_.count(active_connection_->handle())) {
    // Active connection is still registered.
    return;
  }

  if (connections_.empty()) {
    active_connection_.reset();
    ConfigureHci();
    return;
  }

  active_connection_ = connections_.begin()->second.connection;
  ConfigureHci();
}

void ScoDataChannelImpl::ConfigureHci() {
  if (!active_connection_) {
    hci_->ResetSco([](zx_status_t status) {
      bt_log(DEBUG, "hci", "ResetSco completed with status %s", zx_status_get_string(status));
    });
    return;
  }

  hci_spec::SynchronousConnectionParameters params = active_connection_->parameters();

  ScoCodingFormat coding_format;
  if (params.output_coding_format.coding_format == hci_spec::CodingFormat::kMSbc) {
    coding_format = ScoCodingFormat::kMsbc;
  } else if (params.output_coding_format.coding_format == hci_spec::CodingFormat::kCvsd) {
    coding_format = ScoCodingFormat::kCvsd;
  } else {
    bt_log(WARN, "hci", "SCO connection has unsupported coding format, treating as CVSD");
    coding_format = ScoCodingFormat::kCvsd;
  }

  ScoSampleRate sample_rate;
  const uint16_t bits_per_byte = CHAR_BIT;
  uint16_t bytes_per_sample = params.output_coded_data_size_bits / bits_per_byte;
  if (bytes_per_sample == 0) {
    // Err on the side of reserving too much bandwidth in the transport drivers.
    bt_log(WARN, "hci", "SCO connection has unsupported encoding size, treating as 16-bit");
    bytes_per_sample = 2;
  }
  const uint32_t bytes_per_second = params.output_bandwidth;
  const uint32_t samples_per_second = bytes_per_second / bytes_per_sample;
  if (samples_per_second == 8000) {
    sample_rate = ScoSampleRate::k8Khz;
  } else if (samples_per_second == 16000) {
    sample_rate = ScoSampleRate::k16Khz;
  } else {
    // Err on the side of reserving too much bandwidth in the transport drivers.
    bt_log(WARN, "hci", "SCO connection has unsupported sample rate, treating as 16kHz");
    sample_rate = ScoSampleRate::k16Khz;
  }

  ScoEncoding encoding;
  if (params.output_coded_data_size_bits == 8) {
    encoding = ScoEncoding::k8Bits;
  } else if (params.output_coded_data_size_bits == 16) {
    encoding = ScoEncoding::k16Bits;
  } else {
    // Err on the side of reserving too much bandwidth in the transport drivers.
    bt_log(WARN, "hci", "SCO connection has unsupported sample rate, treating as 16-bit");
    encoding = ScoEncoding::k16Bits;
  }

  auto conn = connections_.find(active_connection_->handle());
  ZX_ASSERT(conn != connections_.end());

  auto callback = [self = weak_ptr_factory_.GetWeakPtr(),
                   handle = conn->first](zx_status_t status) {
    if (self) {
      self->OnHciConfigured(handle, status);
    }
  };
  hci_->ConfigureSco(coding_format, encoding, sample_rate, std::move(callback));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ScoDataChannelImpl::OnHciConfigured(hci_spec::ConnectionHandle conn_handle,
                                         zx_status_t status) {
  auto iter = connections_.find(conn_handle);
  if (iter == connections_.end()) {
    // The connection may have been unregistered before the config callback was called.
    return;
  }

  if (status != ZX_OK) {
    bt_log(WARN, "hci", "ConfigureSco failed with status %s (handle: %#.4x)",
           zx_status_get_string(status), conn_handle);
    // The error callback may unregister the connection synchronously, so |iter| should not be
    // used past this line.
    iter->second.connection->OnHciError();
    UnregisterConnection(conn_handle);
    return;
  }

  iter->second.config_state = HciConfigState::kConfigured;
  TrySendNextPackets();
}

std::unique_ptr<ScoDataChannel> ScoDataChannel::Create(const DataBufferInfo& buffer_info,
                                                       CommandChannel* command_channel,
                                                       HciWrapper* hci) {
  return std::make_unique<ScoDataChannelImpl>(buffer_info, command_channel, hci);
}

}  // namespace bt::hci
