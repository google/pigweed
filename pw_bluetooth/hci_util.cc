// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth/hci_util.h"

#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::bluetooth {

pw::Result<size_t> GetHciHeaderSize(emboss::H4PacketType type) {
  switch (type) {
    case emboss::H4PacketType::COMMAND:
      return pw::bluetooth::emboss::CommandHeaderView::SizeInBytes();
    case emboss::H4PacketType::ACL_DATA:
      return pw::bluetooth::emboss::AclDataFrameHeaderView::SizeInBytes();
    case emboss::H4PacketType::SYNC_DATA:
      return pw::bluetooth::emboss::ScoDataHeaderView::SizeInBytes();
    case emboss::H4PacketType::EVENT:
      return pw::bluetooth::emboss::EventHeaderView::SizeInBytes();
    case emboss::H4PacketType::ISO_DATA:
      return pw::bluetooth::emboss::IsoDataFrameHeaderView::SizeInBytes();
    default:
      return pw::Status::InvalidArgument();
  }
}

pw::Result<size_t> GetHciPayloadSize(emboss::H4PacketType type,
                                     pw::span<const uint8_t> hci_header) {
  switch (type) {
    case emboss::H4PacketType::COMMAND: {
      emboss::CommandHeaderView view =
          emboss::MakeCommandHeaderView(hci_header.data(), hci_header.size());
      if (!view.IsComplete()) {
        return pw::Status::OutOfRange();
      }
      return view.parameter_total_size().Read();
    }
    case emboss::H4PacketType::ACL_DATA: {
      emboss::AclDataFrameHeaderView view = emboss::MakeAclDataFrameHeaderView(
          hci_header.data(), hci_header.size());
      if (!view.IsComplete()) {
        return pw::Status::OutOfRange();
      }
      return view.data_total_length().Read();
    }
    case emboss::H4PacketType::SYNC_DATA: {
      emboss::ScoDataHeaderView view =
          emboss::MakeScoDataHeaderView(hci_header.data(), hci_header.size());
      if (!view.IsComplete()) {
        return pw::Status::OutOfRange();
      }
      return view.data_total_length().Read();
    }
    case emboss::H4PacketType::EVENT: {
      emboss::EventHeaderView view =
          emboss::MakeEventHeaderView(hci_header.data(), hci_header.size());
      if (!view.IsComplete()) {
        return pw::Status::OutOfRange();
      }
      return view.parameter_total_size().Read();
    };
    case emboss::H4PacketType::ISO_DATA: {
      emboss::IsoDataFrameHeaderView view = emboss::MakeIsoDataFrameHeaderView(
          hci_header.data(), hci_header.size());
      if (!view.IsComplete()) {
        return pw::Status::OutOfRange();
      }
      return view.data_total_length().Read();
    };
    default:
      return pw::Status::InvalidArgument();
  }
}

}  // namespace pw::bluetooth
