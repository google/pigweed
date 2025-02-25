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

#include "pw_bluetooth_sapphire/fake_lease_provider.h"

#include "pw_unit_test/framework.h"  // IWYU pragma: keep

using Lease = pw::bluetooth_sapphire::Lease;

TEST(FakeLeaseProviderTest, FakeLeaseProvider) {
  pw::bluetooth_sapphire::testing::FakeLeaseProvider provider;
  pw::Result<Lease> lease = PW_SAPPHIRE_ACQUIRE_LEASE(provider, "lease_name");
  ASSERT_TRUE(lease.ok());
  EXPECT_EQ(provider.lease_count(), 1u);
  lease = pw::Status::Unknown();
  EXPECT_EQ(provider.lease_count(), 0u);

  provider.set_acquire_status(pw::Status::Unavailable());
  lease = PW_SAPPHIRE_ACQUIRE_LEASE(provider, "lease_name2");
  EXPECT_EQ(lease.status(), pw::Status::Unavailable());
  EXPECT_EQ(provider.lease_count(), 0u);
}
