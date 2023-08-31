// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "transport.h"

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"

namespace bt::hci {

using FeaturesBits = pw::bluetooth::Controller::FeaturesBits;

Transport::Transport(std::unique_ptr<pw::bluetooth::Controller> controller)
    : WeakSelf(this), controller_(std::move(controller)) {
  BT_ASSERT(controller_);
}

Transport::~Transport() { bt_log(INFO, "hci", "Transport shutting down"); }

void Transport::Initialize(fit::callback<void(bool /*success*/)> complete_callback) {
  BT_ASSERT(!command_channel_);

  bt_log(DEBUG, "hci", "initializing Transport");
  auto self = GetWeakPtr();
  auto complete_cb_wrapper = [self, cb = std::move(complete_callback)](pw::Status status) mutable {
    if (!self.is_alive()) {
      return;
    }

    if (!status.ok()) {
      cb(/*success=*/false);
      return;
    }

    self->command_channel_ =
        std::make_unique<CommandChannel>(self->controller_.get(), self->dispatcher_);
    self->command_channel_->set_channel_timeout_cb([self] {
      if (self.is_alive()) {
        self->OnChannelError();
      }
    });

    self->controller_->GetFeatures([self, cb = std::move(cb)](FeaturesBits features) mutable {
      if (!self.is_alive()) {
        return;
      }
      self->features_ = features;

      bt_log(INFO, "hci", "Transport initialized");
      cb(/*success=*/true);
    });
  };

  auto error_cb = [self](pw::Status status) {
    if (self.is_alive()) {
      self->OnChannelError();
    }
  };

  controller_->Initialize(std::move(complete_cb_wrapper), std::move(error_cb));
}

bool Transport::InitializeACLDataChannel(const DataBufferInfo& bredr_buffer_info,
                                         const DataBufferInfo& le_buffer_info) {
  acl_data_channel_ =
      AclDataChannel::Create(this, controller_.get(), bredr_buffer_info, le_buffer_info);

  if (hci_node_) {
    acl_data_channel_->AttachInspect(hci_node_, AclDataChannel::kInspectNodeName);
  }

  return true;
}

bool Transport::InitializeScoDataChannel(const DataBufferInfo& buffer_info) {
  if (!buffer_info.IsAvailable()) {
    bt_log(WARN, "hci", "failed to initialize SCO data channel: buffer info is not available");
    return false;
  }

  if (static_cast<uint32_t>(*features_ & FeaturesBits::kHciSco) == 0) {
    bt_log(WARN, "hci", "HCI SCO not supported");
    return false;
  }

  sco_data_channel_ =
      ScoDataChannel::Create(buffer_info, command_channel_.get(), controller_.get());
  return true;
}

FeaturesBits Transport::GetFeatures() {
  BT_ASSERT(features_);
  return features_.value();
}

void Transport::SetTransportErrorCallback(fit::closure callback) {
  BT_ASSERT(callback);
  BT_ASSERT(!error_cb_);
  error_cb_ = std::move(callback);
}

void Transport::OnChannelError() {
  bt_log(ERROR, "hci", "channel error, calling Transport error callback");
  // The channels should not be shut down yet. That should be left to higher layers so
  // dependent objects can be destroyed first.
  if (error_cb_) {
    error_cb_();
  }
}

void Transport::AttachInspect(inspect::Node& parent, const std::string& name) {
  BT_ASSERT(acl_data_channel_);
  hci_node_ = parent.CreateChild(name);

  if (command_channel_) {
    command_channel_->AttachInspect(hci_node_);
  }

  if (acl_data_channel_) {
    acl_data_channel_->AttachInspect(hci_node_, AclDataChannel::kInspectNodeName);
  }
}

}  // namespace bt::hci
