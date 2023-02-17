// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller_test_double_base.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

namespace bt::testing {

ControllerTestDoubleBase::ControllerTestDoubleBase() {}

ControllerTestDoubleBase::~ControllerTestDoubleBase() {
  // When this destructor gets called any subclass state will be undefined. If
  // Stop() has not been called before reaching this point this can cause
  // runtime errors when our event loop handlers attempt to invoke the pure
  // virtual methods of this class.
}

bool ControllerTestDoubleBase::SendCommandChannelPacket(const ByteBuffer& packet) {
  if (!event_cb_) {
    return false;
  }

  // Post packet to simulate async behavior that many tests expect.
  DynamicByteBuffer buffer(packet);
  auto self = weak_self_.GetWeakPtr();
  async::PostTask(async_get_default_dispatcher(), [self, buffer = std::move(buffer)]() {
    if (self.is_alive()) {
      self->event_cb_({reinterpret_cast<const std::byte*>(buffer.data()), buffer.size()});
    }
  });
  return true;
}

bool ControllerTestDoubleBase::SendACLDataChannelPacket(const ByteBuffer& packet) {
  if (!acl_cb_) {
    return false;
  }

  // Post packet to simulate async behavior that some tests expect.
  DynamicByteBuffer buffer(packet);
  auto self = weak_self_.GetWeakPtr();
  async::PostTask(async_get_default_dispatcher(), [self, buffer = std::move(buffer)]() {
    if (self.is_alive()) {
      self->acl_cb_({reinterpret_cast<const std::byte*>(buffer.data()), buffer.size()});
    }
  });
  return true;
}

bool ControllerTestDoubleBase::SendScoDataChannelPacket(const ByteBuffer& packet) {
  if (!sco_cb_) {
    return false;
  }

  sco_cb_({reinterpret_cast<const std::byte*>(packet.data()), packet.size()});
  return true;
}

void ControllerTestDoubleBase::Initialize(PwStatusCallback complete_callback,
                                          PwStatusCallback error_callback) {
  error_cb_ = std::move(error_callback);
  complete_callback(PW_STATUS_OK);
}

void ControllerTestDoubleBase::Close(PwStatusCallback callback) {
  event_cb_ = nullptr;
  acl_cb_ = nullptr;
  sco_cb_ = nullptr;
  callback(PW_STATUS_OK);
}

void ControllerTestDoubleBase::ConfigureSco(ScoCodingFormat coding_format, ScoEncoding encoding,
                                            ScoSampleRate sample_rate,
                                            pw::Callback<void(pw::Status)> callback) {
  if (!configure_sco_cb_) {
    return;
  }

  // Post the callback to simulate async behavior with a real controller.
  auto callback_wrapper = [=, cb = std::move(callback)](pw::Status status) mutable {
    async::PostTask(async_get_default_dispatcher(),
                    [status, cb = std::move(cb)]() mutable { cb(status); });
  };
  configure_sco_cb_(coding_format, encoding, sample_rate, std::move(callback_wrapper));
}

void ControllerTestDoubleBase::ResetSco(pw::Callback<void(pw::Status)> callback) {
  if (!reset_sco_cb_) {
    return;
  }

  // Post the callback to simulate async behavior with a real controller.
  auto callback_wrapper = [=, cb = std::move(callback)](pw::Status status) mutable {
    async::PostTask(async_get_default_dispatcher(),
                    [status, cb = std::move(cb)]() mutable { cb(status); });
  };
  reset_sco_cb_(std::move(callback_wrapper));
}

}  // namespace bt::testing
