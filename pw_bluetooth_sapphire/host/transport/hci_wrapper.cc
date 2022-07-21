// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hci_wrapper.h"

#include <fuchsia/hardware/bt/hci/cpp/banjo.h>
#include <fuchsia/hardware/bt/vendor/cpp/banjo.h>
#include <lib/async/cpp/task.h>
#include <lib/async/cpp/wait.h>
#include <lib/fit/defer.h>
#include <lib/zx/channel.h>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"

namespace bt::hci {

namespace {

VendorFeaturesBits BanjoVendorFeaturesToVendorFeaturesBits(bt_vendor_features_t features) {
  VendorFeaturesBits out{0};
  if (features & BT_VENDOR_FEATURES_SET_ACL_PRIORITY_COMMAND) {
    out |= VendorFeaturesBits::kSetAclPriorityCommand;
  }
  if (features & BT_VENDOR_FEATURES_ANDROID_VENDOR_EXTENSIONS) {
    out |= VendorFeaturesBits::kAndroidVendorExtensions;
  }
  return out;
}

}  // namespace

class HciWrapperImpl final : public HciWrapper {
 public:
  HciWrapperImpl(std::unique_ptr<DeviceWrapper> device, async_dispatcher_t* dispatcher);

  ~HciWrapperImpl() override;

  bool Initialize(ErrorCallback error_callback) override;

  zx_status_t SendCommand(std::unique_ptr<CommandPacket> packet) override;

  void SetEventCallback(EventPacketFunction callback) override;

  zx_status_t SendAclPacket(std::unique_ptr<ACLDataPacket> packet) override;

  void SetAclCallback(AclPacketFunction callback) override;

  zx_status_t SendScoPacket(std::unique_ptr<ScoDataPacket> packet) override;

  void SetScoCallback(ScoPacketFunction callback) override;

  bool IsScoSupported() override { return sco_channel_.is_valid(); }

  void ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding, ScoSampleRate sample_rate,
                    StatusCallback callback) override;

  void ResetSco(StatusCallback callback) override;

  VendorFeaturesBits GetVendorFeatures() override;

  fitx::result<zx_status_t, DynamicByteBuffer> EncodeSetAclPriorityCommand(
      hci_spec::ConnectionHandle connection, hci::AclPriority priority) override;

 private:
  // Used by Banjo callbacks to detect stack destruction & to dispatch callbacks onto the bt-host
  // thread.
  struct CallbackData : public fbl::RefCounted<CallbackData> {
    // Lock to guard reads/writes to the |dispatcher| pointer variable below (not the underlying
    // dispatcher). Calls to async::PostTask and async::WaitBase::Begin should be considered reads,
    // and require the lock to be held.
    std::mutex lock;
    // Set to nullptr on HciWrapperImpl destruction to indicate to Banjo callbacks, which may run on
    // an HCI driver thread, that they should do nothing. It is safe to access |dispatcher| on a
    // different thread than |HciWrapperImpl::dispatcher_| because operations on the underying
    // dispatcher, including waiting for signals and posting tasks, are thread-safe. The only
    // concern is that the callbacks would use the dispatcher after it is destroyed and this pointer
    // is invalid, but that is impossible because the dispatcher outlives HciWrapper, and HciWrapper
    // sets |dispatcher| to null upon destruction.
    async_dispatcher_t* dispatcher __TA_GUARDED(lock);
  };

  void OnError(zx_status_t status);

  void CleanUp();

  // Wraps a callback in a callback that posts the callback to the bt-host thread.
  StatusCallback ThreadSafeCallbackWrapper(StatusCallback callback);

  void InitializeWait(async::WaitBase& wait, zx::channel& channel);

  zx_status_t OnChannelReadable(zx_status_t status, async::WaitBase* wait,
                                MutableBufferView buffer_view, size_t header_size,
                                zx::channel& channel, fit::function<uint16_t()> size_from_header);

  void OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal);

  void OnCommandSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                       const zx_packet_signal_t* signal);

  void OnScoSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal);

  std::unique_ptr<DeviceWrapper> device_;

  zx::channel acl_channel_;
  zx::channel command_channel_;
  zx::channel sco_channel_;

  EventPacketFunction event_cb_;
  AclPacketFunction acl_cb_;
  ScoPacketFunction sco_cb_;
  ErrorCallback error_cb_;

  async::WaitMethod<HciWrapperImpl, &HciWrapperImpl::OnAclSignal> acl_wait_{this};
  async::WaitMethod<HciWrapperImpl, &HciWrapperImpl::OnCommandSignal> command_wait_{this};
  async::WaitMethod<HciWrapperImpl, &HciWrapperImpl::OnScoSignal> sco_wait_{this};

  async_dispatcher_t* dispatcher_;

  fbl::RefPtr<CallbackData> callback_data_;
};

HciWrapperImpl::HciWrapperImpl(std::unique_ptr<DeviceWrapper> device,
                               async_dispatcher_t* dispatcher)
    : device_(std::move(device)), dispatcher_(dispatcher) {
  callback_data_ = fbl::AdoptRef(new CallbackData{.dispatcher = dispatcher_});
}

HciWrapperImpl::~HciWrapperImpl() { CleanUp(); }

bool HciWrapperImpl::Initialize(ErrorCallback error_callback) {
  error_cb_ = std::move(error_callback);

  command_channel_ = device_->GetCommandChannel();
  if (!command_channel_.is_valid()) {
    bt_log(ERROR, "hci", "Failed to open command channel");
    return false;
  }

  acl_channel_ = device_->GetACLDataChannel();
  if (!acl_channel_.is_valid()) {
    bt_log(ERROR, "hci", "Failed to open ACL channel");
    return false;
  }

  fitx::result<zx_status_t, zx::channel> sco_result = device_->GetScoChannel();
  if (sco_result.is_ok()) {
    sco_channel_ = std::move(sco_result.value());
  } else {
    // Failing to open a SCO channel is not fatal, it just indicates lack of SCO support.
    bt_log(INFO, "hci", "Failed to open SCO channel: %s",
           zx_status_get_string(sco_result.error_value()));
  }
  return true;
}

zx_status_t HciWrapperImpl::SendCommand(std::unique_ptr<CommandPacket> packet) {
  return command_channel_.write(/*flags=*/0, packet->view().data().data(), packet->view().size(),
                                /*handles=*/nullptr, /*num_handles=*/0);
}

void HciWrapperImpl::SetEventCallback(EventPacketFunction callback) {
  ZX_ASSERT(callback);
  event_cb_ = std::move(callback);
  InitializeWait(command_wait_, command_channel_);
}

zx_status_t HciWrapperImpl::SendAclPacket(std::unique_ptr<ACLDataPacket> packet) {
  return acl_channel_.write(/*flags=*/0, packet->view().data().data(), packet->view().size(),
                            /*handles=*/nullptr, /*num_handles=*/0);
}

void HciWrapperImpl::SetAclCallback(AclPacketFunction callback) {
  acl_cb_ = std::move(callback);
  if (!acl_cb_) {
    acl_wait_.Cancel();
    return;
  }
  InitializeWait(acl_wait_, acl_channel_);
}

zx_status_t HciWrapperImpl::SendScoPacket(std::unique_ptr<ScoDataPacket> packet) {
  return sco_channel_.write(/*flags=*/0, packet->view().data().data(), packet->view().size(),
                            /*handles=*/nullptr, /*num_handles=*/0);
}

void HciWrapperImpl::SetScoCallback(ScoPacketFunction callback) {
  ZX_ASSERT(sco_channel_.is_valid());
  ZX_ASSERT(callback);
  sco_cb_ = std::move(callback);
  InitializeWait(sco_wait_, sco_channel_);
}

void HciWrapperImpl::OnError(zx_status_t status) {
  CleanUp();

  if (error_cb_) {
    error_cb_(status);
  }
}

void HciWrapperImpl::CleanUp() {
  {
    std::lock_guard<std::mutex> guard(callback_data_->lock);
    callback_data_->dispatcher = nullptr;
  }

  // Waits need to be canceled before the underlying channels are destroyed.
  acl_wait_.Cancel();
  command_wait_.Cancel();
  sco_wait_.Cancel();

  acl_channel_.reset();
  sco_channel_.reset();
  command_channel_.reset();
}

HciWrapper::StatusCallback HciWrapperImpl::ThreadSafeCallbackWrapper(StatusCallback callback) {
  return [cb = std::move(callback), data = callback_data_](zx_status_t status) mutable {
    std::lock_guard<std::mutex> guard(data->lock);
    // Don't run the callback if HciWrapper has been destroyed.
    if (data->dispatcher) {
      // This callback may be run on a different thread, so post the result callback to the
      // bt-host thread.
      async::PostTask(data->dispatcher, [cb = std::move(cb), status]() mutable { cb(status); });
    }
  };
}

void HciWrapperImpl::InitializeWait(async::WaitBase& wait, zx::channel& channel) {
  ZX_ASSERT(channel.is_valid());
  wait.Cancel();
  wait.set_object(channel.get());
  wait.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
  ZX_ASSERT(wait.Begin(dispatcher_) == ZX_OK);
}

void HciWrapperImpl::ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding,
                                  ScoSampleRate sample_rate, StatusCallback callback) {
  device_->ConfigureSco(
      static_cast<uint8_t>(coding_format), static_cast<uint8_t>(encoding),
      static_cast<uint8_t>(sample_rate),
      [](void* ctx, zx_status_t status) {
        std::unique_ptr<StatusCallback> callback(static_cast<StatusCallback*>(ctx));
        (*callback)(status);
      },
      new StatusCallback(ThreadSafeCallbackWrapper(std::move(callback))));
}

void HciWrapperImpl::ResetSco(StatusCallback callback) {
  device_->ResetSco(
      [](void* ctx, zx_status_t status) {
        std::unique_ptr<StatusCallback> callback(static_cast<StatusCallback*>(ctx));
        (*callback)(status);
      },
      new StatusCallback(ThreadSafeCallbackWrapper(std::move(callback))));
}

VendorFeaturesBits HciWrapperImpl::GetVendorFeatures() {
  return BanjoVendorFeaturesToVendorFeaturesBits(device_->GetVendorFeatures());
}

fitx::result<zx_status_t, DynamicByteBuffer> HciWrapperImpl::EncodeSetAclPriorityCommand(
    hci_spec::ConnectionHandle connection, hci::AclPriority priority) {
  bt_vendor_set_acl_priority_params_t priority_params = {
      .connection_handle = connection,
      .priority = static_cast<bt_vendor_acl_priority_t>((priority == AclPriority::kNormal)
                                                            ? BT_VENDOR_ACL_PRIORITY_NORMAL
                                                            : BT_VENDOR_ACL_PRIORITY_HIGH),
      .direction = static_cast<bt_vendor_acl_direction_t>((priority == AclPriority::kSource)
                                                              ? BT_VENDOR_ACL_DIRECTION_SOURCE
                                                              : BT_VENDOR_ACL_DIRECTION_SINK)};
  bt_vendor_params_t cmd_params = {.set_acl_priority = priority_params};

  std::optional<DynamicByteBuffer> encode_result =
      device_->EncodeVendorCommand(BT_VENDOR_COMMAND_SET_ACL_PRIORITY, cmd_params);
  if (!encode_result) {
    bt_log(WARN, "hci", "Failed to encode vendor command");
    return fitx::error(ZX_ERR_INTERNAL);
  }

  return fitx::ok(std::move(encode_result.value()));
}

zx_status_t HciWrapperImpl::OnChannelReadable(zx_status_t status, async::WaitBase* wait,
                                              MutableBufferView buffer_view, size_t header_size,
                                              zx::channel& channel,
                                              fit::function<uint16_t()> size_from_header) {
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "channel error: %s", zx_status_get_string(status));
    return ZX_ERR_IO;
  }

  uint32_t read_size;
  zx_status_t read_status =
      channel.read(0u, buffer_view.mutable_data(), /*handles=*/nullptr, buffer_view.size(), 0,
                   &read_size, /*actual_handles=*/nullptr);
  if (read_status != ZX_OK) {
    bt_log(DEBUG, "hci", "failed to read RX bytes: %s", zx_status_get_string(read_status));
    // Stop receiving packets.
    return ZX_ERR_IO;
  }

  // The wait needs to be restarted after every signal.
  auto defer_wait = fit::defer([wait, this] {
    zx_status_t status = wait->Begin(dispatcher_);
    if (status != ZX_OK) {
      bt_log(ERROR, "hci", "wait error: %s", zx_status_get_string(status));
    }
  });

  if (read_size < header_size) {
    bt_log(ERROR, "hci", "malformed packet - expected at least %zu bytes, got %u", header_size,
           read_size);
    return ZX_ERR_IO_DATA_INTEGRITY;
  }

  const size_t payload_size = read_size - header_size;
  const uint16_t expected_payload_size = size_from_header();
  if (payload_size != expected_payload_size) {
    bt_log(ERROR, "hci",
           "malformed packet - payload size from header (%hu) does not match"
           " received payload size: %zu",
           expected_payload_size, payload_size);
    return ZX_ERR_IO_DATA_INTEGRITY;
  }

  return ZX_OK;
}

void HciWrapperImpl::OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                 zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "HciWrapperImpl::OnAclSignal");

  if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
    OnError(ZX_ERR_PEER_CLOSED);
    return;
  }
  ZX_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  // Allocate a buffer for the packet. Since we don't know the size beforehand
  // we allocate the largest possible buffer.
  auto packet = ACLDataPacket::New(slab_allocators::kLargeACLDataPayloadSize);
  if (!packet) {
    bt_log(ERROR, "hci", "failed to allocate ACL data packet");
    return;
  }

  auto size_from_header = [&packet] { return le16toh(packet->view().header().data_total_length); };
  const zx_status_t read_status =
      OnChannelReadable(status, wait, packet->mutable_view()->mutable_data(),
                        sizeof(hci_spec::ACLDataHeader), acl_channel_, size_from_header);
  if (read_status == ZX_ERR_IO_DATA_INTEGRITY) {
    // TODO(fxbug.dev/97362): Handle these types of errors by calling error_cb_.
    bt_log(ERROR, "hci", "Received invalid ACL packet; dropping");
    return;
  }
  if (read_status != ZX_OK) {
    OnError(read_status);
    return;
  }

  packet->InitializeFromBuffer();
  acl_cb_(std::move(packet));
}

void HciWrapperImpl::OnCommandSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                     zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "HciWrapperImpl::OnCommandSignal");

  if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
    bt_log(ERROR, "hci", "command channel closed");
    OnError(ZX_ERR_PEER_CLOSED);
    return;
  }
  ZX_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  // Allocate a buffer for the packet. Since we don't know the size beforehand
  // we allocate the largest possible buffer.
  std::unique_ptr<EventPacket> packet = EventPacket::New(slab_allocators::kLargeControlPayloadSize);
  if (!packet) {
    bt_log(ERROR, "hci", "failed to allocate event packet");
    OnError(ZX_ERR_NO_MEMORY);
    return;
  }

  auto size_from_header = [&packet] { return packet->view().header().parameter_total_size; };

  const zx_status_t read_status =
      OnChannelReadable(status, wait, packet->mutable_view()->mutable_data(),
                        sizeof(hci_spec::EventHeader), command_channel_, size_from_header);

  if (read_status == ZX_ERR_IO_DATA_INTEGRITY) {
    // TODO(fxbug.dev/97362): Handle these types of errors by calling error_cb_.
    bt_log(ERROR, "hci", "Received invalid event packet; dropping");
    return;
  }
  if (read_status != ZX_OK) {
    bt_log(ERROR, "hci", "failed to read event packet");
    OnError(read_status);
    return;
  }

  packet->InitializeFromBuffer();
  event_cb_(std::move(packet));
}

void HciWrapperImpl::OnScoSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                 zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "HciWrapperImpl::OnScoSignal");

  if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
    OnError(ZX_ERR_PEER_CLOSED);
    return;
  }
  ZX_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  // Allocate a buffer for the packet. Since we don't know the size beforehand
  // we allocate the largest possible buffer.
  std::unique_ptr<ScoDataPacket> packet =
      ScoDataPacket::New(hci_spec::kMaxSynchronousDataPacketPayloadSize);
  if (!packet) {
    bt_log(ERROR, "hci", "failed to allocate SCO packet");
    OnError(ZX_ERR_NO_MEMORY);
    return;
  }

  auto size_from_header = [&packet] { return packet->view().header().data_total_length; };
  const zx_status_t read_status =
      OnChannelReadable(status, wait, packet->mutable_view()->mutable_data(),
                        sizeof(hci_spec::SynchronousDataHeader), sco_channel_, size_from_header);
  if (read_status == ZX_ERR_IO_DATA_INTEGRITY) {
    // TODO(fxbug.dev/97362): Handle these types of errors by calling error_cb_.
    bt_log(ERROR, "hci", "Received invalid SCO packet; dropping");
    return;
  }
  if (read_status != ZX_OK) {
    OnError(read_status);
    return;
  }

  packet->InitializeFromBuffer();

  sco_cb_(std::move(packet));
}

std::unique_ptr<HciWrapper> HciWrapper::Create(std::unique_ptr<DeviceWrapper> device,
                                               async_dispatcher_t* dispatcher) {
  return std::make_unique<HciWrapperImpl>(std::move(device), dispatcher);
}

}  // namespace bt::hci
