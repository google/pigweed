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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_group.h"

#include "pw_bluetooth_sapphire/internal/host/iso/fake_iso_stream.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"

namespace bt::iso {
namespace {

using testing::FakeIsoStream;

// FakeCigStreamCreator
// Holds an unordered_map of `FakeIsoStream`s, accessible from outside
// with the const `streams()` accessor.
// Implements CigStreamCreator.
class FakeCigStreamCreator : public CigStreamCreator {
 public:
  FakeCigStreamCreator() : weak_self_(this) {}
  ~FakeCigStreamCreator() override = default;

  WeakSelf<IsoStream>::WeakPtr CreateCisConfiguration(
      CigCisIdentifier id,
      hci_spec::ConnectionHandle cis_handle,
      CisEstablishedCallback on_established_cb,
      pw::Callback<void()> on_closed_cb) override {
    auto stream = std::make_unique<FakeIsoStream>(
        cis_handle, std::move(on_established_cb), std::move(on_closed_cb));
    auto weak_stream = stream->GetWeakPtr();
    streams_.emplace(id, std::move(stream));
    return weak_stream;
  }

  const std::unordered_map<CigCisIdentifier, std::unique_ptr<FakeIsoStream>>&
  streams() const {
    return streams_;
  }

  CigStreamCreator::WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  std::unordered_map<CigCisIdentifier, std::unique_ptr<FakeIsoStream>> streams_;
  WeakSelf<FakeCigStreamCreator> weak_self_;
};

class IsoGroupTest : public bt::testing::FakeDispatcherControllerTest<
                         bt::testing::MockController> {
 public:
  IsoGroupTest() = default;
  ~IsoGroupTest() override = default;

  void SetUp() override {
    bt::testing::FakeDispatcherControllerTest<
        bt::testing::MockController>::SetUp();
    cig_stream_creator_ = std::make_unique<FakeCigStreamCreator>();
  }

  void TearDown() override {
    cig_stream_creator_.reset();
    bt::testing::FakeDispatcherControllerTest<
        bt::testing::MockController>::TearDown();
  }

 protected:
  std::unique_ptr<FakeCigStreamCreator> cig_stream_creator_;
};

TEST_F(IsoGroupTest, CreateCig) {
  hci_spec::CigIdentifier kCigId = 0x01;
  bool on_closed_cb_called = false;
  auto cig =
      IsoGroup::CreateCig(kCigId,
                          transport()->GetWeakPtr(),
                          cig_stream_creator_->GetWeakPtr(),
                          [&](IsoGroup&) { on_closed_cb_called = true; });

  ASSERT_TRUE(cig);
  EXPECT_EQ(cig->id(), kCigId);
  EXPECT_FALSE(on_closed_cb_called);
  EXPECT_TRUE(cig_stream_creator_->streams().empty());
}

}  // namespace
}  // namespace bt::iso
