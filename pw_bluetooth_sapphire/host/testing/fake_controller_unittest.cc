// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"

#include <gtest/gtest.h>
#include <pw_async/fake_dispatcher_fixture.h>
#include <pw_bluetooth/hci_events.emb.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"

namespace bt::testing {

using FakeControllerTest = pw::async::test::FakeDispatcherFixture;

TEST_F(FakeControllerTest, TestInquiryCommand) {
  FakeController controller(dispatcher());

  int event_cb_count = 0;
  controller.SetEventFunction([&event_cb_count](
                                  pw::span<const std::byte> packet_bytes) {
    auto header_view = pw::bluetooth::emboss::MakeEventHeaderView(
        reinterpret_cast<const uint8_t*>(packet_bytes.data()),
        packet_bytes.size());

    if (header_view.event_code().Read() == hci_spec::kCommandStatusEventCode) {
      std::unique_ptr<hci::EventPacket> event = hci::EventPacket::New(
          packet_bytes.size() - sizeof(hci_spec::EventHeader));
      event->mutable_view()->mutable_data().Write(
          reinterpret_cast<const uint8_t*>(packet_bytes.data()),
          packet_bytes.size());
      event->InitializeFromBuffer();
      pw::bluetooth::emboss::StatusCode status;
      event->ToStatusCode(&status);
      EXPECT_EQ(status, pw::bluetooth::emboss::StatusCode::SUCCESS);
      ++event_cb_count;
    }

    else if (header_view.event_code().Read() ==
             hci_spec::kInquiryCompleteEventCode) {
      auto inquiry_complete_view =
          pw::bluetooth::emboss::MakeInquiryCompleteEventView(
              reinterpret_cast<const uint8_t*>(packet_bytes.data()),
              packet_bytes.size());
      EXPECT_EQ(inquiry_complete_view.status().Read(),
                pw::bluetooth::emboss::StatusCode::SUCCESS);
      ++event_cb_count;
    }

    else {
      ADD_FAILURE() << "Unexpected Event packet received";
    }
  });

  auto inquiry = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::InquiryCommandWriter>(hci_spec::kInquiry);
  auto view = inquiry.view_t();
  view.lap().Write(pw::bluetooth::emboss::InquiryAccessCode::GIAC);
  view.inquiry_length().Write(8);
  view.num_responses().Write(0);
  controller.SendCommand(inquiry.data().subspan());

  // The maximum amount of time before Inquiry is halted is calculated as
  // inquiry_length * 1.28 s. FakeController:OnInquiry simulates this by posting
  // the InquiryCompleteEvent to be returned after this duration.
  RunFor(std::chrono::milliseconds(
      static_cast<int64_t>(view.inquiry_length().Read()) * 1280));

  EXPECT_EQ(event_cb_count, 2);
}

}  // namespace bt::testing
