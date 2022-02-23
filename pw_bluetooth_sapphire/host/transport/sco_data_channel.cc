// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_data_channel.h"

#include <lib/async/default.h>
#include <zircon/status.h>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "device_wrapper.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {
class ScoDataChannelImpl final : public ScoDataChannel {
 public:
  ScoDataChannelImpl(zx::channel channel, const DataBufferInfo& buffer_info, Transport* transport,
                     DeviceWrapper* device);
  ~ScoDataChannelImpl() override;

  // ScoDataChannel overrides:
  void RegisterConnection(fxl::WeakPtr<ConnectionInterface> connection) override;
  void UnregisterConnection(hci_spec::ConnectionHandle handle) override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override;
  void OnOutboundPacketReadable() override;
  uint16_t max_data_length() const override { return buffer_info_.max_data_length(); }

 private:
  // HCI callbacks may be executed on a different thread, and they may be called after
  // ScoDataChannelImpl is destroyed. Callbacks are called using the C-style BtHci Banjo interface,
  // which only supports a raw context/cookie pointer. This is essentially a C-compatible
  // std::weak_ptr.
  struct ConfigCallbackData : public fbl::RefCounted<ConfigCallbackData> {
    explicit ConfigCallbackData(ScoDataChannelImpl* ptr, hci_spec::ConnectionHandle handle)
        : self(ptr), handle(handle) {}
    std::mutex lock;
    ScoDataChannelImpl* self __TA_GUARDED(lock);
    hci_spec::ConnectionHandle handle __TA_GUARDED(lock);
  };

  enum class HciConfigState {
    kPending,
    kConfigured,
  };

  struct ConnectionData {
    fxl::WeakPtr<ConnectionInterface> connection;
    // config_cb_data == nullptr implies the config callback has completed.
    fbl::RefPtr<ConfigCallbackData> config_cb_data;
    HciConfigState config_state = HciConfigState::kPending;
  };

  // Read Ready Handler for |channel_|
  void OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                      const zx_packet_signal_t* signal);

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

  fitx::result<zx_status_t, std::unique_ptr<ScoDataPacket>> ReadPacketFromChannel();

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

  Transport* transport_;
  DeviceWrapper* device_;
  zx::channel channel_;
  DataBufferInfo buffer_info_;

  async_dispatcher_t* dispatcher_;

  std::unordered_map<hci_spec::ConnectionHandle, ConnectionData> connections_;

  // Only 1 connection may send packets at a time.
  fxl::WeakPtr<ConnectionInterface> active_connection_;

  // Stores per-connection counts of unacknowledged packets sent to the controller. Entries are
  // updated/removed on the HCI Number Of Completed Packets event and removed when a connection is
  // unregistered (the controller does not acknowledge packets of disconnected links).
  std::unordered_map<hci_spec::ConnectionHandle, size_t> pending_packet_counts_;

  async::WaitMethod<ScoDataChannelImpl, &ScoDataChannelImpl::OnChannelReady> channel_wait_{this};

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_;
};

ScoDataChannelImpl::ScoDataChannelImpl(zx::channel channel, const DataBufferInfo& buffer_info,
                                       Transport* transport, DeviceWrapper* device)
    : transport_(transport),
      device_(device),
      channel_(std::move(channel)),
      buffer_info_(buffer_info),
      dispatcher_(async_get_default_dispatcher()),
      channel_wait_(this, channel_.get(), ZX_CHANNEL_READABLE) {
  // ScoDataChannel shouldn't be used if the buffer is unavailable (implying the controller
  // doesn't support SCO).
  ZX_ASSERT(buffer_info_.IsAvailable());

  num_completed_packets_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      hci_spec::kNumberOfCompletedPacketsEventCode,
      fit::bind_member(this, &ScoDataChannelImpl::OnNumberOfCompletedPacketsEvent));
  ZX_ASSERT(num_completed_packets_event_handler_id_);

  channel_wait_.Begin(dispatcher_);
}

ScoDataChannelImpl::~ScoDataChannelImpl() {
  transport_->command_channel()->RemoveEventHandler(num_completed_packets_event_handler_id_);

  // Prevent use-after-free in configuration callbacks.
  for (auto& [_, conn_data] : connections_) {
    if (conn_data.config_cb_data) {
      std::lock_guard guard(conn_data.config_cb_data->lock);
      conn_data.config_cb_data->self = nullptr;
    }
  }
}

void ScoDataChannelImpl::RegisterConnection(fxl::WeakPtr<ConnectionInterface> connection) {
  ZX_ASSERT(connection->parameters().output_data_path == hci_spec::ScoDataPath::kHci);
  ConnectionData conn_data{
      .connection = connection,
      .config_cb_data = fbl::AdoptRef(new ConfigCallbackData(this, connection->handle()))};
  auto [_, inserted] = connections_.emplace(connection->handle(), std::move(conn_data));
  ZX_ASSERT_MSG(inserted, "connection with handle %#.4x already registered", connection->handle());
  MaybeUpdateActiveConnection();
}

void ScoDataChannelImpl::UnregisterConnection(hci_spec::ConnectionHandle handle) {
  auto iter = connections_.find(handle);
  if (iter == connections_.end()) {
    return;
  }

  if (iter->second.config_cb_data) {
    // Ensure the configuration callback does nothing.
    std::lock_guard guard(iter->second.config_cb_data->lock);
    iter->second.config_cb_data->self = nullptr;
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

void ScoDataChannelImpl::OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                        zx_status_t status, const zx_packet_signal_t* signal) {
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "channel error: %s", zx_status_get_string(status));
    return;
  }

  ZX_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  for (size_t count = 0; count < signal->count; count++) {
    fitx::result<zx_status_t, std::unique_ptr<ScoDataPacket>> result = ReadPacketFromChannel();
    if (result.is_error()) {
      // Ignore malformed packets.
      bt_log(ERROR, "hci", "ignoring packet due to read error: %s",
             zx_status_get_string(result.error_value()));
      continue;
    }
    auto conn_iter = connections_.find(letoh16(result.value()->connection_handle()));
    if (conn_iter == connections_.end()) {
      // Ignore inbound packets for connections that aren't registered. Unlike ACL, buffering data
      // received before a connection is registered is unnecessary for SCO (it's realtime and
      // not expected to be reliable).
      bt_log(DEBUG, "hci", "ignoring inbound SCO packet for unregistered connection: %#.4x",
             result.value()->connection_handle());
      continue;
    }
    conn_iter->second.connection->ReceiveInboundPacket(std::move(result.value()));
  }

  // The wait needs to be restarted after every signal.
  status = wait->Begin(dispatcher);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "wait error: %s", zx_status_get_string(status));
  }
}

fitx::result<zx_status_t, std::unique_ptr<ScoDataPacket>>
ScoDataChannelImpl::ReadPacketFromChannel() {
  // Allocate a buffer for the event. Since we don't know the size beforehand
  // we allocate the largest possible buffer.
  std::unique_ptr<ScoDataPacket> packet =
      ScoDataPacket::New(hci_spec::kMaxSynchronousDataPacketPayloadSize);
  if (!packet) {
    bt_log(ERROR, "hci", "failed to allocate buffer for received SCO data packet");
    return fitx::error(ZX_ERR_NO_MEMORY);
  }

  uint32_t read_size;
  MutableBufferView packet_bytes = packet->mutable_view()->mutable_data();
  const zx_status_t read_status = channel_.read(
      /*flags=*/0u, packet_bytes.mutable_data(), /*handles=*/nullptr, packet_bytes.size(),
      /*num_handles=*/0u, &read_size, /*actual_handles=*/nullptr);
  if (read_status != ZX_OK) {
    bt_log(ERROR, "hci", "failed to read inbound SCO bytes: %s", zx_status_get_string(read_status));
    return fitx::error(ZX_ERR_IO);
  }

  if (read_size < sizeof(hci_spec::SynchronousDataHeader)) {
    bt_log(ERROR, "hci", "malformed data packet - expected at least %zu bytes, got %u",
           sizeof(hci_spec::SynchronousDataHeader), read_size);
    return fitx::error(ZX_ERR_INVALID_ARGS);
  }

  const size_t rx_payload_size = read_size - sizeof(hci_spec::SynchronousDataHeader);
  const uint8_t size_from_header = packet->view().header().data_total_length;
  if (size_from_header != rx_payload_size) {
    bt_log(ERROR, "hci",
           "malformed packet - payload size from header (%hhu) does not match"
           " received payload size: %zu",
           size_from_header, rx_payload_size);
    return fitx::error(ZX_ERR_INVALID_ARGS);
  }

  packet->InitializeFromBuffer();
  return fitx::ok(std::move(packet));
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
  for (auto& [_, conn_data] : connections_) {
    for (size_t num_free_packets = GetNumFreePackets(); num_free_packets != 0u;
         num_free_packets--) {
      std::unique_ptr<ScoDataPacket> packet = conn_data.connection->GetNextOutboundPacket();
      if (!packet) {
        // This connection has no more packets available.
        break;
      }

      zx_status_t status = channel_.write(0, packet->view().data().data(), packet->view().size(),
                                          /*handles=*/nullptr, /*num_handles=*/0);
      if (status != ZX_OK) {
        bt_log(ERROR, "hci", "failed to send data packet to HCI driver (%s) - dropping packet",
               zx_status_get_string(status));
        continue;
      }

      auto [iter, _] = pending_packet_counts_.try_emplace(packet->connection_handle(), 0u);
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
    device_->ResetSco(
        [](void* ctx, zx_status_t status) {
          bt_log(DEBUG, "hci", "ResetSco completed with status %s", zx_status_get_string(status));
        },
        /*cookie=*/nullptr);
    return;
  }

  hci_spec::SynchronousConnectionParameters params = active_connection_->parameters();

  sco_coding_format_t coding_format;
  if (params.output_coding_format.coding_format == hci_spec::CodingFormat::kMSbc) {
    coding_format = SCO_CODING_FORMAT_MSBC;
  } else if (params.output_coding_format.coding_format == hci_spec::CodingFormat::kCvsd) {
    coding_format = SCO_CODING_FORMAT_CVSD;
  } else {
    bt_log(WARN, "hci", "SCO connection has unsupported coding format, treating as CVSD");
    coding_format = SCO_CODING_FORMAT_CVSD;
  }

  sco_sample_rate_t sample_rate;
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
    sample_rate = SCO_SAMPLE_RATE_KHZ_8;
  } else if (samples_per_second == 16000) {
    sample_rate = SCO_SAMPLE_RATE_KHZ_16;
  } else {
    // Err on the side of reserving too much bandwidth in the transport drivers.
    bt_log(WARN, "hci", "SCO connection has unsupported sample rate, treating as 16kHz");
    sample_rate = SCO_SAMPLE_RATE_KHZ_16;
  }

  sco_encoding_t encoding;
  if (params.output_coded_data_size_bits == 8) {
    encoding = SCO_ENCODING_BITS_8;
  } else if (params.output_coded_data_size_bits == 16) {
    encoding = SCO_ENCODING_BITS_16;
  } else {
    // Err on the side of reserving too much bandwidth in the transport drivers.
    bt_log(WARN, "hci", "SCO connection has unsupported sample rate, treating as 16-bit");
    encoding = SCO_ENCODING_BITS_16;
  }

  bt_hci_configure_sco_callback callback = [](void* ctx, zx_status_t status) {
    fbl::RefPtr<ConfigCallbackData> data =
        fbl::ImportFromRawPtr(static_cast<ConfigCallbackData*>(ctx));
    std::lock_guard guard(data->lock);
    if (data->self) {
      data->self->OnHciConfigured(data->handle, status);
    }
  };

  // Leak a config_cb_data ref to the callback, where it will be imported.
  auto conn = connections_.find(active_connection_->handle());
  ZX_ASSERT(conn != connections_.end());
  fbl::RefPtr<ConfigCallbackData> ref = conn->second.config_cb_data;
  device_->ConfigureSco(coding_format, encoding, sample_rate, callback,
                        /*cookie=*/ExportToRawPtr(&ref));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ScoDataChannelImpl::OnHciConfigured(hci_spec::ConnectionHandle conn_handle,
                                         zx_status_t status) {
  // Post task for thread safety. This method may be called from an HCI driver thread.
  async::PostTask(dispatcher_, [this, conn_handle, status] {
    auto iter = connections_.find(conn_handle);
    if (iter == connections_.end()) {
      // The connection may have been unregistered before the config callback was called.
      return;
    }
    iter->second.config_cb_data.reset();

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
  });
}

std::unique_ptr<ScoDataChannel> ScoDataChannel::Create(zx::channel channel,
                                                       const DataBufferInfo& buffer_info,
                                                       Transport* transport,
                                                       DeviceWrapper* device_wrapper) {
  return std::make_unique<ScoDataChannelImpl>(std::move(channel), buffer_info, transport,
                                              device_wrapper);
}

}  // namespace bt::hci
