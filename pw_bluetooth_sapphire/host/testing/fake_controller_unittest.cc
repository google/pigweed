// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"

#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>
#include <pw_async_fuchsia/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

#include <pw_bluetooth/hci_events.emb.h>

namespace bt::testing {

class FakeControllerTest : public ::gtest::TestLoopFixture {
 public:
  pw::async::Dispatcher& pw_dispatcher() { return pw_dispatcher_; }

 private:
  pw::async::fuchsia::FuchsiaDispatcher pw_dispatcher_{dispatcher()};
};

TEST_F(FakeControllerTest, TestInquiryCommand) {
  FakeController controller(pw_dispatcher());

  int event_cb_count = 0;
  controller.SetEventFunction([&event_cb_count](pw::span<const std::byte> packet_bytes) {
    auto header_view = pw::bluetooth::emboss::MakeEventHeaderView(
        reinterpret_cast<const uint8_t*>(packet_bytes.data()), packet_bytes.size());

    if (header_view.event_code().Read() == hci_spec::kCommandStatusEventCode) {
      std::unique_ptr<hci::EventPacket> event =
          hci::EventPacket::New(packet_bytes.size() - sizeof(hci_spec::EventHeader));
      event->mutable_view()->mutable_data().Write(
          reinterpret_cast<const uint8_t*>(packet_bytes.data()), packet_bytes.size());
      event->InitializeFromBuffer();
      pw::bluetooth::emboss::StatusCode status;
      event->ToStatusCode(&status);
      EXPECT_EQ(status, pw::bluetooth::emboss::StatusCode::SUCCESS);
      ++event_cb_count;
    }

    else if (header_view.event_code().Read() == hci_spec::kInquiryCompleteEventCode) {
      auto inquiry_complete_view = pw::bluetooth::emboss::MakeInquiryCompleteEventView(
          reinterpret_cast<const uint8_t*>(packet_bytes.data()), packet_bytes.size());
      EXPECT_EQ(inquiry_complete_view.status().Read(), pw::bluetooth::emboss::StatusCode::SUCCESS);
      ++event_cb_count;
    }

    else {
      ADD_FAILURE() << "Unexpected Event packet received";
    }
  });

  auto inquiry = hci::EmbossCommandPacket::New<pw::bluetooth::emboss::InquiryCommandWriter>(
      hci_spec::kInquiry);
  auto view = inquiry.view_t();
  view.lap().Write(pw::bluetooth::emboss::InquiryAccessCode::GIAC);
  view.inquiry_length().Write(8);
  view.num_responses().Write(0);
  controller.SendCommand(inquiry.data().subspan());

  // The maximum amount of time before Inquiry is halted is calculated as inquiry_length * 1.28 s.
  // FakeController:OnInquiry simulates this by posting the InquiryCompleteEvent to be returned
  // after this duration.
  RunLoopFor(zx::msec(static_cast<int64_t>(view.inquiry_length().Read()) * 1280));

  EXPECT_EQ(event_cb_count, 2);
}

}  // namespace bt::testing
