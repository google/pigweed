// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include "lib/gtest/test_loop_fixture.h"

namespace bthost {

namespace {

class ProfileServerTest : public ::gtest::TestLoopFixture {
 public:
  ProfileServerTest() = default;
  ~ProfileServerTest() override = default;

 protected:
  void SetUp() override {
    profile_server_.emplace(fxl::WeakPtr<bt::gap::Adapter>(),
                            client_.NewRequest(async_get_default_dispatcher()));
  }

  void TearDown() override {
    profile_server_.reset();
    RunLoopUntilIdle();  // Run all pending tasks.
  }

  fuchsia::bluetooth::bredr::Profile* client() { return client_.get(); }

 private:
  std::optional<ProfileServer> profile_server_;
  fidl::InterfacePtr<fuchsia::bluetooth::bredr::Profile> client_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(ProfileServerTest);
};

TEST_F(ProfileServerTest, ErrorOnInvalidUuid) {
  bool called = false;
  fuchsia::bluetooth::Status status;
  uint64_t service_id;
  auto cb = [&](auto s, uint64_t id) {
    called = true;
    status = std::move(s);
    service_id = id;
  };

  fuchsia::bluetooth::bredr::ServiceDefinition def;
  def.service_class_uuids.emplace_back("bogus_uuid");

  client()->AddService(std::move(def),
                       fuchsia::bluetooth::bredr::SecurityLevel::ENCRYPTION_OPTIONAL, false,
                       std::move(cb));

  RunLoopUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_TRUE(status.error);
}

}  // namespace
}  // namespace bthost
