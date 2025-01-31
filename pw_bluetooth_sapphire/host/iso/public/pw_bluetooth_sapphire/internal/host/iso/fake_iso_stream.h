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

#pragma once

#include <pw_assert/check.h>

#include <optional>

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream.h"

namespace bt::iso::testing {

// Testing replacement for IsoStream with functionality built up as needed.
class FakeIsoStream : public IsoStream {
 public:
  FakeIsoStream() : weak_self_(this) {}

  // IsoStream overrides
  bool OnCisEstablished(const hci::EventPacket& event) override { return true; }

  void SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection direction,
      const bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>& codec_id,
      const std::optional<std::vector<uint8_t>>& codec_configuration,
      uint32_t controller_delay_usecs,
      SetupDataPathCallback&& on_complete_cb,
      IncomingDataHandler&& on_incoming_data_available_cb) override {
    on_incoming_data_available_cb_ = std::move(on_incoming_data_available_cb);
    on_complete_cb(setup_data_path_status_);
  }

  void ReceiveInboundPacket(const pw::span<const std::byte> packet) override {}

  hci_spec::ConnectionHandle cis_handle() const override { return 0; }

  void Close() override {}

  std::unique_ptr<IsoDataPacket> ReadNextQueuedIncomingPacket() override {
    if (incoming_packet_queue_.size() < 1) {
      return nullptr;
    }
    std::unique_ptr<IsoDataPacket> next_frame =
        std::move(incoming_packet_queue_.front());
    incoming_packet_queue_.pop();
    return next_frame;
  }

  void Send(pw::ConstByteSpan) override {}

  IsoStream::WeakPtr GetWeakPtr() override { return weak_self_.GetWeakPtr(); }

  // Testing functionality
  void SetSetupDataPathReturnStatus(IsoStream::SetupDataPathError status) {
    setup_data_path_status_ = status;
  }

  void QueueIncomingFrame(std::unique_ptr<IsoDataPacket> frame) {
    incoming_packet_queue_.push(std::move(frame));
  }

  size_t incoming_packet_requests() { return incoming_packet_requests_; }

  bool NotifyClientOfPacketReceived(pw::span<const std::byte> packet) {
    PW_CHECK(on_incoming_data_available_cb_.has_value());
    return (*on_incoming_data_available_cb_)(packet);
  }

 protected:
  IsoStream::SetupDataPathError setup_data_path_status_ =
      IsoStream::SetupDataPathError::kSuccess;

 private:
  std::optional<IncomingDataHandler> on_incoming_data_available_cb_;
  std::queue<std::unique_ptr<IsoDataPacket>> incoming_packet_queue_;
  size_t incoming_packet_requests_ = 0;
  WeakSelf<FakeIsoStream> weak_self_;
};

}  // namespace bt::iso::testing
