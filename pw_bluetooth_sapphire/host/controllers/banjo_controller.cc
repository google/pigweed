// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "banjo_controller.h"

#include <lib/async/cpp/task.h>
#include <lib/fit/defer.h>

#include <variant>

#include "helpers.h"
#include "pw_result/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/trace.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/slab_allocators.h"

namespace bt::controllers {

namespace {

sco_coding_format_t ScoCodingFormatToBanjo(
    pw::bluetooth::Controller::ScoCodingFormat coding_format) {
  switch (coding_format) {
    case pw::bluetooth::Controller::ScoCodingFormat::kCvsd:
      return SCO_CODING_FORMAT_CVSD;
    case pw::bluetooth::Controller::ScoCodingFormat::kMsbc:
      return SCO_CODING_FORMAT_MSBC;
    default:
      BT_PANIC("invalid SCO coding format");
  }
}

sco_encoding_t ScoEncodingToBanjo(pw::bluetooth::Controller::ScoEncoding encoding) {
  switch (encoding) {
    case pw::bluetooth::Controller::ScoEncoding::k8Bits:
      return SCO_ENCODING_BITS_8;
    case pw::bluetooth::Controller::ScoEncoding::k16Bits:
      return SCO_ENCODING_BITS_16;
    default:
      BT_PANIC("invalid SCO encoding");
  }
}

sco_sample_rate_t ScoSampleRateToBanjo(pw::bluetooth::Controller::ScoSampleRate sample_rate) {
  switch (sample_rate) {
    case pw::bluetooth::Controller::ScoSampleRate::k8Khz:
      return SCO_SAMPLE_RATE_KHZ_8;
    case pw::bluetooth::Controller::ScoSampleRate::k16Khz:
      return SCO_SAMPLE_RATE_KHZ_16;
    default:
      BT_PANIC("invalid SCO sample rate");
  }
}

bt_vendor_acl_priority_t AclPriorityToBanjo(pw::bluetooth::AclPriority priority) {
  switch (priority) {
    case pw::bluetooth::AclPriority::kNormal:
      return BT_VENDOR_ACL_PRIORITY_NORMAL;
    case pw::bluetooth::AclPriority::kSink:
    case pw::bluetooth::AclPriority::kSource:
      return BT_VENDOR_ACL_PRIORITY_HIGH;
  }
}

bt_vendor_acl_direction_t AclPriorityToBanjoAclDirection(pw::bluetooth::AclPriority priority) {
  switch (priority) {
    // The direction for kNormal is arbitrary.
    case pw::bluetooth::AclPriority::kNormal:
    case pw::bluetooth::AclPriority::kSink:
      return BT_VENDOR_ACL_DIRECTION_SINK;
    case pw::bluetooth::AclPriority::kSource:
      return BT_VENDOR_ACL_DIRECTION_SOURCE;
  }
}

}  // namespace

BanjoController::BanjoController(ddk::BtHciProtocolClient hci_proto,
                                 std::optional<ddk::BtVendorProtocolClient> vendor_proto,
                                 async_dispatcher_t* dispatcher)
    : hci_proto_(hci_proto),
      vendor_proto_(vendor_proto),
      dispatcher_(dispatcher),
      callback_data_(fbl::AdoptRef(new CallbackData{.dispatcher = dispatcher})) {}

BanjoController::~BanjoController() { CleanUp(); }

void BanjoController::Initialize(PwStatusCallback complete_callback,
                                 PwStatusCallback error_callback) {
  error_cb_ = std::move(error_callback);

  zx::channel their_command_chan;
  zx_status_t status = zx::channel::create(0, &command_channel_, &their_command_chan);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to create command channel");
    complete_callback(pw::Status::Internal());
    return;
  }

  status = hci_proto_.OpenCommandChannel(std::move(their_command_chan));
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to open command channel");
    complete_callback(pw::Status::Internal());
    return;
  }
  InitializeWait(command_wait_, command_channel_);

  zx::channel their_acl_chan;
  status = zx::channel::create(0, &acl_channel_, &their_acl_chan);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to create ACL channel");
    complete_callback(pw::Status::Internal());
    return;
  }

  status = hci_proto_.OpenAclDataChannel(std::move(their_acl_chan));
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to open ACL channel");
    complete_callback(pw::Status::Internal());
    return;
  }
  InitializeWait(acl_wait_, acl_channel_);

  zx::channel their_sco_chan;
  status = zx::channel::create(0, &sco_channel_, &their_sco_chan);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to create SCO channel");
    complete_callback(pw::Status::Internal());
    return;
  }

  status = hci_proto_.OpenScoChannel(std::move(their_sco_chan));
  if (status == ZX_OK) {
    InitializeWait(sco_wait_, sco_channel_);
  } else {
    // Failing to open a SCO channel is not fatal, it just indicates lack of SCO support.
    bt_log(INFO, "controllers", "Failed to open SCO channel: %s", zx_status_get_string(status));
    sco_channel_.reset();
  }

  complete_callback(PW_STATUS_OK);
}

void BanjoController::Close(PwStatusCallback callback) {
  CleanUp();
  callback(PW_STATUS_OK);
}

void BanjoController::SendCommand(pw::span<const std::byte> command) {
  zx_status_t status =
      command_channel_.write(/*flags=*/0, command.data(), static_cast<uint32_t>(command.size()),
                             /*handles=*/nullptr, /*num_handles=*/0);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "failed to write command channel: %s",
           zx_status_get_string(status));
    OnError(status);
  }
}

void BanjoController::SendAclData(pw::span<const std::byte> data) {
  zx_status_t status =
      acl_channel_.write(/*flags=*/0, data.data(), static_cast<uint32_t>(data.size()),
                         /*handles=*/nullptr, /*num_handles=*/0);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "failed to write ACL channel: %s", zx_status_get_string(status));
    OnError(status);
  }
}

void BanjoController::SendScoData(pw::span<const std::byte> data) {
  zx_status_t status =
      sco_channel_.write(/*flags=*/0, data.data(), static_cast<uint32_t>(data.size()),
                         /*handles=*/nullptr, /*num_handles=*/0);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "failed to write SCO channel: %s", zx_status_get_string(status));
    OnError(status);
  }
}

void BanjoController::ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding,
                                   ScoSampleRate sample_rate, PwStatusCallback callback) {
  hci_proto_.ConfigureSco(
      ScoCodingFormatToBanjo(coding_format), ScoEncodingToBanjo(encoding),
      ScoSampleRateToBanjo(sample_rate),
      [](void* ctx, zx_status_t status) {
        std::unique_ptr<PwStatusCallback> callback(static_cast<PwStatusCallback*>(ctx));
        (*callback)(ZxStatusToPwStatus(status));
      },
      new PwStatusCallback(ThreadSafeCallbackWrapper(std::move(callback))));
}

void BanjoController::ResetSco(pw::Callback<void(pw::Status)> callback) {
  hci_proto_.ResetSco(
      [](void* ctx, zx_status_t status) {
        std::unique_ptr<PwStatusCallback> callback(static_cast<PwStatusCallback*>(ctx));
        (*callback)(ZxStatusToPwStatus(status));
      },
      new PwStatusCallback(ThreadSafeCallbackWrapper(std::move(callback))));
}

void BanjoController::GetFeatures(pw::Callback<void(FeaturesBits)> callback) {
  if (!vendor_proto_) {
    callback(FeaturesBits{0});
    return;
  }
  callback(BanjoVendorFeaturesToFeaturesBits(vendor_proto_->GetFeatures()));
}

void BanjoController::EncodeVendorCommand(
    pw::bluetooth::VendorCommandParameters parameters,
    pw::Callback<void(pw::Result<pw::span<const std::byte>>)> callback) {
  BT_ASSERT(vendor_proto_.has_value());

  if (!std::holds_alternative<pw::bluetooth::SetAclPriorityCommandParameters>(parameters)) {
    callback(pw::Status::Unimplemented());
    return;
  }

  pw::bluetooth::SetAclPriorityCommandParameters params =
      std::get<pw::bluetooth::SetAclPriorityCommandParameters>(parameters);

  bt_vendor_set_acl_priority_params_t priority_params = {
      .connection_handle = params.connection_handle,
      .priority = AclPriorityToBanjo(params.priority),
      .direction = AclPriorityToBanjoAclDirection(params.priority)};
  bt_vendor_params_t cmd_params = {.set_acl_priority = priority_params};

  StaticByteBuffer<pw::bluetooth::kMaxVendorCommandBufferSize> encoded_command;
  size_t actual_size = 0;
  zx_status_t encode_result = vendor_proto_->EncodeCommand(
      BT_VENDOR_COMMAND_SET_ACL_PRIORITY, &cmd_params, encoded_command.mutable_data(),
      encoded_command.size(), &actual_size);

  if (encode_result != ZX_OK) {
    bt_log(WARN, "controllers", "Failed to encode vendor command");
    callback(ZxStatusToPwStatus(encode_result));
    return;
  }
  callback(encoded_command.subspan(/*pos=*/0, actual_size));
}

void BanjoController::OnError(zx_status_t status) {
  CleanUp();

  if (error_cb_) {
    error_cb_(ZxStatusToPwStatus(status));
  }
}

void BanjoController::CleanUp() {
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

// Wraps a callback in one that posts on the bt-host thread.
BanjoController::PwStatusCallback BanjoController::ThreadSafeCallbackWrapper(
    PwStatusCallback callback) {
  return [cb = std::move(callback), data = callback_data_](pw::Status status) mutable {
    std::lock_guard<std::mutex> guard(data->lock);
    // Don't run the callback if BanjoController has been destroyed.
    if (data->dispatcher) {
      // This callback may be run on a different thread, so post the result callback to the
      // bt-host thread.
      async::PostTask(data->dispatcher, [cb = std::move(cb), status]() mutable { cb(status); });
    }
  };
}

void BanjoController::InitializeWait(async::WaitBase& wait, zx::channel& channel) {
  BT_ASSERT(channel.is_valid());
  wait.Cancel();
  wait.set_object(channel.get());
  wait.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
  BT_ASSERT(wait.Begin(dispatcher_) == ZX_OK);
}

void BanjoController::OnChannelSignal(const char* chan_name, zx_status_t status,
                                      async::WaitBase* wait, const zx_packet_signal_t* signal,
                                      pw::span<std::byte> buffer, zx::channel& channel,
                                      DataFunction& data_cb) {
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "%s channel error: %s", chan_name, zx_status_get_string(status));
    OnError(status);
    return;
  }

  if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
    bt_log(ERROR, "controllers", "%s channel closed", chan_name);
    OnError(ZX_ERR_PEER_CLOSED);
    return;
  }
  BT_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  uint32_t read_size;
  zx_status_t read_status = channel.read(0u, buffer.data(), /*handles=*/nullptr,
                                         static_cast<uint32_t>(buffer.size()), 0, &read_size,
                                         /*actual_handles=*/nullptr);
  if (read_status != ZX_OK) {
    bt_log(ERROR, "controllers", "%s channel: failed to read RX bytes: %s", chan_name,
           zx_status_get_string(read_status));
    OnError(read_status);
    return;
  }
  if (data_cb) {
    data_cb(buffer.subspan(0, read_size));
  } else {
    bt_log(WARN, "controllers", "Dropping packet received on %s channel (no rx callback set)",
           chan_name);
  }

  // The wait needs to be restarted after every signal.
  status = wait->Begin(dispatcher_);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "%s wait error: %s", chan_name, zx_status_get_string(status));
    OnError(status);
  }
}

void BanjoController::OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                  zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "BanjoController::OnAclSignal");

  // Allocate a buffer for the packet. Since we don't know the size beforehand we allocate the
  // largest possible buffer.
  std::byte packet[hci::allocators::kLargeACLDataPacketSize];
  OnChannelSignal("ACL", status, wait, signal, packet, acl_channel_, acl_cb_);
}

void BanjoController::OnCommandSignal(async_dispatcher_t* /*dispatcher*/, async::WaitBase* wait,
                                      zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "BanjoController::OnCommandSignal");

  // Allocate a buffer for the packet. Since we don't know the size beforehand we allocate the
  // largest possible buffer.
  std::byte packet[hci_spec::kMaxEventPacketPayloadSize + sizeof(hci_spec::EventHeader)];
  OnChannelSignal("command", status, wait, signal, packet, command_channel_, event_cb_);
}

void BanjoController::OnScoSignal(async_dispatcher_t* /*dispatcher*/, async::WaitBase* wait,
                                  zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "BanjoController::OnScoSignal");

  // Allocate a buffer for the packet. Since we don't know the size beforehand we allocate the
  // largest possible buffer.
  std::byte packet[hci::allocators::kMaxScoDataPacketSize];
  OnChannelSignal("SCO", status, wait, signal, packet, sco_channel_, sco_cb_);
}

pw::bluetooth::Controller::FeaturesBits BanjoController::BanjoVendorFeaturesToFeaturesBits(
    bt_vendor_features_t features) {
  FeaturesBits out{0};
  if (features & BT_VENDOR_FEATURES_SET_ACL_PRIORITY_COMMAND) {
    out |= FeaturesBits::kSetAclPriorityCommand;
  }
  if (features & BT_VENDOR_FEATURES_ANDROID_VENDOR_EXTENSIONS) {
    out |= FeaturesBits::kAndroidVendorExtensions;
  }
  if (sco_channel_.is_valid()) {
    out |= FeaturesBits::kHciSco;
  }
  return out;
}

}  // namespace bt::controllers
