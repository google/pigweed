// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_adapter.h"

namespace bt::gap::testing {

FakeAdapter::FakeAdapter()
    : init_state_(InitState::kNotInitialized),
      fake_bredr_(std::make_unique<FakeBrEdr>()),
      weak_ptr_factory_(this) {}

bool FakeAdapter::Initialize(InitializeCallback callback, fit::closure transport_closed_callback) {
  init_state_ = InitState::kInitializing;
  async::PostTask(async_get_default_dispatcher(), [this, cb = std::move(callback)] {
    init_state_ = InitState::kInitialized;
    cb(/*success=*/true);
  });
  return true;
}

void FakeAdapter::ShutDown() { init_state_ = InitState::kNotInitialized; }

void FakeAdapter::SetLocalName(std::string name, hci::StatusCallback callback) {
  local_name_ = name;
  callback(hci::Status());
}

void FakeAdapter::SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback) {
  device_class_ = dev_class;
  callback(hci::Status());
}

}  // namespace bt::gap::testing
