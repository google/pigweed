// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"

#include "gtest/gtest.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_class.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt {
namespace gap {
namespace {

using common::CreateStaticByteBuffer;
using common::DeviceAddress;
using common::MutableBufferView;
using common::StaticByteBuffer;

// All fields are initialized to zero as they are unused in these tests.
const hci::LEConnectionParameters kTestParams;

// Arbitrary ID value used by the bonding tests below. The actual value of this
// constant does not effect the test logic.
constexpr PeerId kId(100);
constexpr int8_t kTestRSSI = 10;

const DeviceAddress kAddrBrEdr(DeviceAddress::Type::kBREDR,
                               "AA:BB:CC:DD:EE:FF");
const DeviceAddress kAddrLePublic(DeviceAddress::Type::kLEPublic,
                                  "01:02:03:04:05:06");
// LE Public Device Address that has the same value as a BR/EDR BD_ADDR, e.g. on
// a dual-mode device.
const DeviceAddress kAddrLeAlias(DeviceAddress::Type::kLEPublic,
                                 "AA:BB:CC:DD:EE:FF");

// TODO(armansito): Make these adhere to privacy specification.
const DeviceAddress kAddrLeRandom(DeviceAddress::Type::kLERandom,
                                  "06:05:04:03:02:01");
const DeviceAddress kAddrLeRandom2(DeviceAddress::Type::kLERandom,
                                   "FF:EE:DD:CC:BB:AA");
const DeviceAddress kAddrLeAnon(DeviceAddress::Type::kLEAnonymous,
                                "06:05:04:03:02:01");

const auto kAdvData =
    CreateStaticByteBuffer(0x05,  // Length
                           0x09,  // AD type: Complete Local Name
                           'T', 'e', 's', 't');
const auto kEirData = kAdvData;

const bt::sm::LTK kLTK;
const bt::sm::Key kKey{};

const bt::sm::LTK kBrEdrKey;

// Phone (Networking)
const common::DeviceClass kTestDeviceClass({0x06, 0x02, 0x02});

class GAP_PeerCacheTest : public ::gtest::TestLoopFixture {
 public:
  void SetUp() override { TestLoopFixture::SetUp(); }

  void TearDown() override {
    RunLoopUntilIdle();
    TestLoopFixture::TearDown();
  }

 protected:
  // Creates a new Peer, and caches a pointer to that peer.
  __WARN_UNUSED_RESULT bool NewPeer(const DeviceAddress& addr,
                                    bool connectable) {
    auto* peer = cache_.NewPeer(addr, connectable);
    if (!peer) {
      return false;
    }
    peer_ = peer;
    return true;
  }

  PeerCache* cache() { return &cache_; }
  // Returns the cached pointer to the peer created in the most recent call to
  // NewPeer(). The caller must ensure that the peer has not expired out of
  // the cache. (Tests of cache expiration should generally subclass the
  // GAP_PeerCacheExpirationTest fixture.)
  Peer* peer() { return peer_; }

 private:
  PeerCache cache_;
  Peer* peer_;
};

TEST_F(GAP_PeerCacheTest, LookUp) {
  auto kAdvData0 = CreateStaticByteBuffer(0x05, 0x09, 'T', 'e', 's', 't');
  auto kAdvData1 = CreateStaticByteBuffer(0x0C, 0x09, 'T', 'e', 's', 't', ' ',
                                          'D', 'e', 'v', 'i', 'c', 'e');

  // These should return false regardless of the input while the cache is empty.
  EXPECT_FALSE(cache()->FindByAddress(kAddrLePublic));
  EXPECT_FALSE(cache()->FindById(kId));

  auto peer = cache()->NewPeer(kAddrLePublic, true);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(TechnologyType::kLowEnergy, peer->technology());
  EXPECT_TRUE(peer->connectable());
  EXPECT_TRUE(peer->temporary());
  EXPECT_EQ(kAddrLePublic, peer->address());
  EXPECT_EQ(0u, peer->le()->advertising_data().size());
  EXPECT_EQ(hci::kRSSIInvalid, peer->rssi());

  // A look up should return the same instance.
  EXPECT_EQ(peer, cache()->FindById(peer->identifier()));
  EXPECT_EQ(peer, cache()->FindByAddress(peer->address()));

  // Adding a peer with the same address should return nullptr.
  EXPECT_FALSE(cache()->NewPeer(kAddrLePublic, true));

  peer->MutLe().SetAdvertisingData(kTestRSSI, kAdvData1);
  EXPECT_TRUE(
      common::ContainersEqual(kAdvData1, peer->le()->advertising_data()));
  EXPECT_EQ(kTestRSSI, peer->rssi());

  peer->MutLe().SetAdvertisingData(kTestRSSI, kAdvData0);
  EXPECT_TRUE(
      common::ContainersEqual(kAdvData0, peer->le()->advertising_data()));
  EXPECT_EQ(kTestRSSI, peer->rssi());
}

TEST_F(GAP_PeerCacheTest, LookUpBrEdrPeerByLePublicAlias) {
  ASSERT_FALSE(cache()->FindByAddress(kAddrLeAlias));
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  auto* p = cache()->FindByAddress(kAddrBrEdr);
  ASSERT_TRUE(p);
  EXPECT_EQ(peer(), p);

  p = cache()->FindByAddress(kAddrLeAlias);
  ASSERT_TRUE(p);
  EXPECT_EQ(peer(), p);
  EXPECT_EQ(DeviceAddress::Type::kBREDR, p->address().type());
}

TEST_F(GAP_PeerCacheTest, LookUpLePeerByBrEdrAlias) {
  EXPECT_FALSE(cache()->FindByAddress(kAddrBrEdr));
  ASSERT_TRUE(NewPeer(kAddrLeAlias, true));
  auto* p = cache()->FindByAddress(kAddrLeAlias);
  ASSERT_TRUE(p);
  EXPECT_EQ(peer(), p);

  p = cache()->FindByAddress(kAddrBrEdr);
  ASSERT_TRUE(p);
  EXPECT_EQ(peer(), p);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, p->address().type());
}

TEST_F(GAP_PeerCacheTest, NewPeerDoesNotCrashWhenNoCallbackIsRegistered) {
  PeerCache().NewPeer(kAddrLePublic, true);
}

TEST_F(GAP_PeerCacheTest, ForEachEmpty) {
  bool found = false;
  cache()->ForEach([&](const auto&) { found = true; });
  EXPECT_FALSE(found);
}

TEST_F(GAP_PeerCacheTest, ForEach) {
  int count = 0;
  ASSERT_TRUE(NewPeer(kAddrLePublic, true));
  cache()->ForEach([&](const auto& p) {
    count++;
    EXPECT_EQ(peer()->identifier(), p.identifier());
    EXPECT_EQ(peer()->address(), p.address());
  });
  EXPECT_EQ(1, count);
}

TEST_F(GAP_PeerCacheTest, NewPeerInvokesCallbackWhenPeerIsFirstRegistered) {
  bool was_called = false;
  cache()->set_peer_updated_callback(
      [&was_called](const auto&) { was_called = true; });
  cache()->NewPeer(kAddrLePublic, true);
  EXPECT_TRUE(was_called);
}

TEST_F(GAP_PeerCacheTest, NewPeerDoesNotInvokeCallbackWhenPeerIsReRegistered) {
  int call_count = 0;
  cache()->set_peer_updated_callback(
      [&call_count](const auto&) { ++call_count; });
  cache()->NewPeer(kAddrLePublic, true);
  cache()->NewPeer(kAddrLePublic, true);
  EXPECT_EQ(1, call_count);
}

TEST_F(GAP_PeerCacheTest, NewPeerIdentityKnown) {
  EXPECT_TRUE(cache()->NewPeer(kAddrBrEdr, true)->identity_known());
  EXPECT_TRUE(cache()->NewPeer(kAddrLePublic, true)->identity_known());
  EXPECT_FALSE(cache()->NewPeer(kAddrLeRandom, true)->identity_known());
  EXPECT_FALSE(cache()->NewPeer(kAddrLeAnon, false)->identity_known());
}

TEST_F(GAP_PeerCacheTest, NewPeerInitialTechnologyIsClassic) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));

  // A peer initialized with a BR/EDR address should start out as a
  // classic-only.
  ASSERT_TRUE(peer());
  EXPECT_TRUE(peer()->bredr());
  EXPECT_FALSE(peer()->le());
  EXPECT_TRUE(peer()->identity_known());
  EXPECT_EQ(TechnologyType::kClassic, peer()->technology());
}

TEST_F(GAP_PeerCacheTest, NewPeerInitialTechnologyLowEnergy) {
  // LE address types should initialize the peer as LE-only.
  auto* le_publ_peer = cache()->NewPeer(kAddrLePublic, true /*connectable*/);
  auto* le_rand_peer = cache()->NewPeer(kAddrLeRandom, true /*connectable*/);
  auto* le_anon_peer = cache()->NewPeer(kAddrLeAnon, false /*connectable*/);
  ASSERT_TRUE(le_publ_peer);
  ASSERT_TRUE(le_rand_peer);
  ASSERT_TRUE(le_anon_peer);
  EXPECT_TRUE(le_publ_peer->le());
  EXPECT_TRUE(le_rand_peer->le());
  EXPECT_TRUE(le_anon_peer->le());
  EXPECT_FALSE(le_publ_peer->bredr());
  EXPECT_FALSE(le_rand_peer->bredr());
  EXPECT_FALSE(le_anon_peer->bredr());
  EXPECT_EQ(TechnologyType::kLowEnergy, le_publ_peer->technology());
  EXPECT_EQ(TechnologyType::kLowEnergy, le_rand_peer->technology());
  EXPECT_EQ(TechnologyType::kLowEnergy, le_anon_peer->technology());
  EXPECT_TRUE(le_publ_peer->identity_known());
  EXPECT_FALSE(le_rand_peer->identity_known());
  EXPECT_FALSE(le_anon_peer->identity_known());
}

TEST_F(GAP_PeerCacheTest, DisallowNewLowEnergyPeerIfBrEdrPeerExists) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));

  // Try to add new LE peer with a public identity address containing the same
  // value as the existing BR/EDR peer's BD_ADDR.
  auto* le_alias_peer = cache()->NewPeer(kAddrLeAlias, true);
  EXPECT_FALSE(le_alias_peer);
}

TEST_F(GAP_PeerCacheTest, DisallowNewBrEdrPeerIfLowEnergyPeerExists) {
  ASSERT_TRUE(NewPeer(kAddrLeAlias, true));

  // Try to add new BR/EDR peer with BD_ADDR containing the same value as the
  // existing LE peer's public identity address.
  auto* bredr_alias_peer = cache()->NewPeer(kAddrBrEdr, true);
  ASSERT_FALSE(bredr_alias_peer);
}

TEST_F(GAP_PeerCacheTest, BrEdrPeerBecomesDualModeWithAdvertisingData) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_TRUE(peer()->bredr());
  ASSERT_FALSE(peer()->le());

  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
  EXPECT_TRUE(peer()->le());
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());

  // Searching by LE address should turn up this peer, which should retain its
  // original address type.
  auto* const le_peer = cache()->FindByAddress(kAddrLeAlias);
  ASSERT_EQ(peer(), le_peer);
  EXPECT_EQ(DeviceAddress::Type::kBREDR, peer()->address().type());
}

TEST_F(GAP_PeerCacheTest, BrEdrPeerBecomesDualModeWhenConnectedOverLowEnergy) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_TRUE(peer()->bredr());
  ASSERT_FALSE(peer()->le());

  peer()->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);
  EXPECT_TRUE(peer()->le());
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());

  auto* const le_peer = cache()->FindByAddress(kAddrLeAlias);
  ASSERT_EQ(peer(), le_peer);
  EXPECT_EQ(DeviceAddress::Type::kBREDR, peer()->address().type());
}

TEST_F(GAP_PeerCacheTest, BrEdrPeerBecomesDualModeWithLowEnergyConnParams) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_TRUE(peer()->bredr());
  ASSERT_FALSE(peer()->le());

  peer()->MutLe().SetConnectionParameters({});
  EXPECT_TRUE(peer()->le());
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());

  auto* const le_peer = cache()->FindByAddress(kAddrLeAlias);
  ASSERT_EQ(peer(), le_peer);
  EXPECT_EQ(DeviceAddress::Type::kBREDR, peer()->address().type());
}

TEST_F(GAP_PeerCacheTest,
       BrEdrPeerBecomesDualModeWithLowEnergyPreferredConnParams) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_TRUE(peer()->bredr());
  ASSERT_FALSE(peer()->le());

  peer()->MutLe().SetPreferredConnectionParameters({});
  EXPECT_TRUE(peer()->le());
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());

  auto* const le_peer = cache()->FindByAddress(kAddrLeAlias);
  ASSERT_EQ(peer(), le_peer);
  EXPECT_EQ(DeviceAddress::Type::kBREDR, peer()->address().type());
}

TEST_F(GAP_PeerCacheTest, LowEnergyPeerBecomesDualModeWithInquiryData) {
  ASSERT_TRUE(NewPeer(kAddrLeAlias, true));
  ASSERT_TRUE(peer()->le());
  ASSERT_FALSE(peer()->bredr());

  hci::InquiryResult ir;
  ir.bd_addr = kAddrLeAlias.value();
  peer()->MutBrEdr().SetInquiryData(ir);
  EXPECT_TRUE(peer()->bredr());
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());

  // Searching by only BR/EDR technology should turn up this peer, which
  // should still retain its original address type.
  auto* const bredr_peer = cache()->FindByAddress(kAddrBrEdr);
  ASSERT_EQ(peer(), bredr_peer);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, peer()->address().type());
  EXPECT_EQ(kAddrBrEdr, peer()->bredr()->address());
}

TEST_F(GAP_PeerCacheTest,
       LowEnergyPeerBecomesDualModeWhenConnectedOverClassic) {
  ASSERT_TRUE(NewPeer(kAddrLeAlias, true));
  ASSERT_TRUE(peer()->le());
  ASSERT_FALSE(peer()->bredr());

  peer()->MutBrEdr().SetConnectionState(Peer::ConnectionState::kConnected);
  EXPECT_TRUE(peer()->bredr());
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());

  auto* const bredr_peer = cache()->FindByAddress(kAddrBrEdr);
  ASSERT_EQ(peer(), bredr_peer);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, peer()->address().type());
  EXPECT_EQ(kAddrBrEdr, peer()->bredr()->address());
}

class GAP_PeerCacheTest_BondingTest : public GAP_PeerCacheTest {
 public:
  void SetUp() override {
    GAP_PeerCacheTest::SetUp();
    ASSERT_TRUE(NewPeer(kAddrLePublic, true));
    bonded_callback_called_ = false;
    cache()->set_peer_bonded_callback(
        [this](const auto&) { bonded_callback_called_ = true; });
    updated_callback_count_ = 0;
    cache()->set_peer_updated_callback(
        [this](auto&) { updated_callback_count_++; });
  }

  void TearDown() override {
    cache()->set_peer_updated_callback(nullptr);
    updated_callback_count_ = 0;
    cache()->set_peer_bonded_callback(nullptr);
    bonded_callback_called_ = false;
    GAP_PeerCacheTest::TearDown();
  }

 protected:
  bool bonded_callback_called() const { return bonded_callback_called_; }

  // Returns 0 at the beginning of each test case.
  int updated_callback_count() const { return updated_callback_count_; }

 private:
  bool bonded_callback_called_;
  int updated_callback_count_;
};

TEST_F(GAP_PeerCacheTest_BondingTest, AddBondedPeerFailsWithExistingId) {
  sm::PairingData data;
  data.ltk = kLTK;
  EXPECT_FALSE(
      cache()->AddBondedPeer(peer()->identifier(), kAddrLeRandom, data, {}));
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest, AddBondedPeerFailsWithExistingAddress) {
  sm::PairingData data;
  data.ltk = kLTK;
  EXPECT_FALSE(cache()->AddBondedPeer(kId, peer()->address(), data, {}));
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest,
       AddBondedLowEnergyPeerFailsWithExistingBrEdrAliasAddress) {
  EXPECT_TRUE(NewPeer(kAddrBrEdr, true));
  sm::PairingData data;
  data.ltk = kLTK;
  EXPECT_FALSE(cache()->AddBondedPeer(kId, kAddrLeAlias, data, {}));
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest,
       AddBondedBrEdrPeerFailsWithExistingLowEnergyAliasAddress) {
  EXPECT_TRUE(NewPeer(kAddrLeAlias, true));
  EXPECT_FALSE(cache()->AddBondedPeer(kId, kAddrBrEdr, {}, kBrEdrKey));
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest, AddBondedPeerFailsWithoutMandatoryKeys) {
  sm::PairingData data;
  EXPECT_FALSE(cache()->AddBondedPeer(kId, kAddrLeAlias, data, kBrEdrKey));
  data.ltk = kLTK;
  EXPECT_FALSE(cache()->AddBondedPeer(kId, kAddrBrEdr, data, {}));
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest, AddLowEnergyBondedPeerSuccess) {
  sm::PairingData data;
  data.ltk = kLTK;

  EXPECT_TRUE(cache()->AddBondedPeer(kId, kAddrLeRandom, data, {}));
  auto* peer = cache()->FindById(kId);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer, cache()->FindByAddress(kAddrLeRandom));
  EXPECT_EQ(kId, peer->identifier());
  EXPECT_EQ(kAddrLeRandom, peer->address());
  EXPECT_TRUE(peer->identity_known());
  ASSERT_TRUE(peer->le());
  EXPECT_TRUE(peer->le()->bonded());
  ASSERT_TRUE(peer->le()->bond_data());
  EXPECT_EQ(data, *peer->le()->bond_data());
  EXPECT_FALSE(peer->bredr());
  EXPECT_EQ(TechnologyType::kLowEnergy, peer->technology());

  // The "new bond" callback should not be called when restoring a previously
  // bonded peer.
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest, AddBrEdrBondedPeerSuccess) {
  PeerId kId(5);
  sm::PairingData data;

  EXPECT_TRUE(cache()->AddBondedPeer(kId, kAddrBrEdr, data, kBrEdrKey));
  auto* peer = cache()->FindById(kId);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer, cache()->FindByAddress(kAddrBrEdr));
  EXPECT_EQ(kId, peer->identifier());
  EXPECT_EQ(kAddrBrEdr, peer->address());
  EXPECT_TRUE(peer->identity_known());
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(peer->bredr()->bonded());
  ASSERT_TRUE(peer->bredr()->link_key());
  EXPECT_EQ(kBrEdrKey, *peer->bredr()->link_key());
  EXPECT_FALSE(peer->le());
  EXPECT_EQ(TechnologyType::kClassic, peer->technology());

  // The "new bond" callback should not be called when restoring a previously
  // bonded peer.
  EXPECT_FALSE(bonded_callback_called());
}

TEST_F(GAP_PeerCacheTest_BondingTest,
       AddBondedPeerWithIrkIsAddedToResolvingList) {
  sm::PairingData data;
  data.ltk = kLTK;
  data.irk = sm::Key(sm::SecurityProperties(), common::RandomUInt128());

  EXPECT_TRUE(cache()->AddBondedPeer(kId, kAddrLeRandom, data, {}));
  auto* peer = cache()->FindByAddress(kAddrLeRandom);
  ASSERT_TRUE(peer);
  EXPECT_EQ(kAddrLeRandom, peer->address());

  // Looking up the peer by RPA generated using the IRK should return the same
  // peer.
  DeviceAddress rpa = sm::util::GenerateRpa(data.irk->value());
  EXPECT_EQ(peer, cache()->FindByAddress(rpa));
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreLowEnergyBondFailsWithNoKeys) {
  sm::PairingData data;
  EXPECT_FALSE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreLowEnergyBondPeerUnknown) {
  sm::PairingData data;
  data.ltk = kLTK;
  EXPECT_FALSE(cache()->StoreLowEnergyBond(kId, data));
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreLowEnergyBondWithLtk) {
  ASSERT_TRUE(peer()->temporary());
  ASSERT_TRUE(peer()->le());
  ASSERT_FALSE(peer()->le()->bonded());

  sm::PairingData data;
  data.ltk = kLTK;
  EXPECT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));

  EXPECT_TRUE(bonded_callback_called());
  EXPECT_FALSE(peer()->temporary());
  EXPECT_TRUE(peer()->le()->bonded());
  EXPECT_TRUE(peer()->le()->bond_data());
  EXPECT_EQ(data, *peer()->le()->bond_data());
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreLowEnergyBondWithCsrk) {
  ASSERT_TRUE(peer()->temporary());
  ASSERT_TRUE(peer()->le());
  ASSERT_FALSE(peer()->le()->bonded());

  sm::PairingData data;
  data.csrk = kKey;
  EXPECT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));

  EXPECT_TRUE(bonded_callback_called());
  EXPECT_FALSE(peer()->temporary());
  EXPECT_TRUE(peer()->le()->bonded());
  EXPECT_TRUE(peer()->le()->bond_data());
  EXPECT_EQ(data, *peer()->le()->bond_data());
}

// StoreLowEnergyBond fails if it contains the address of a different,
// previously known peer.
TEST_F(GAP_PeerCacheTest_BondingTest,
       StoreLowEnergyBondWithExistingDifferentIdentity) {
  auto* p = cache()->NewPeer(kAddrLeRandom, true);

  // Assign the other peer's address as identity.
  sm::PairingData data;
  data.ltk = kLTK;
  data.identity_address = peer()->address();
  EXPECT_FALSE(cache()->StoreLowEnergyBond(p->identifier(), data));
  EXPECT_FALSE(p->le()->bonded());
  EXPECT_TRUE(p->temporary());
}

// StoreLowEnergyBond fails if the new identity is the address of a "different"
// (another peer record with a distinct ID) BR/EDR peer.
TEST_F(GAP_PeerCacheTest_BondingTest,
       StoreLowEnergyBondWithNewIdentityMatchingExistingBrEdrPeer) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_TRUE(NewPeer(kAddrLeRandom, true));
  ASSERT_FALSE(peer()->identity_known());

  sm::PairingData data;
  data.ltk = kLTK;
  // new identity address is same as another peer's BR/EDR identity
  data.identity_address = kAddrLeAlias;
  const auto old_address = peer()->address();
  ASSERT_EQ(peer(), cache()->FindByAddress(old_address));
  ASSERT_NE(peer(), cache()->FindByAddress(*data.identity_address));
  EXPECT_FALSE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  EXPECT_FALSE(peer()->identity_known());
}

// StoreLowEnergyBond succeeds if it contains an identity address that already
// matches the target peer.
TEST_F(GAP_PeerCacheTest_BondingTest,
       StoreLowEnergyBondWithExistingMatchingIdentity) {
  sm::PairingData data;
  data.ltk = kLTK;
  data.identity_address = peer()->address();
  EXPECT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  EXPECT_TRUE(peer()->le()->bonded());
  EXPECT_EQ(peer(), cache()->FindByAddress(*data.identity_address));
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreLowEnergyBondWithNewIdentity) {
  ASSERT_TRUE(NewPeer(kAddrLeRandom, true));
  ASSERT_FALSE(peer()->identity_known());

  sm::PairingData data;
  data.ltk = kLTK;
  data.identity_address = kAddrLeRandom2;  // assign a new identity address
  const auto old_address = peer()->address();
  ASSERT_EQ(peer(), cache()->FindByAddress(old_address));
  ASSERT_EQ(nullptr, cache()->FindByAddress(*data.identity_address));

  EXPECT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  EXPECT_TRUE(peer()->le()->bonded());

  // Address should have been updated.
  ASSERT_NE(*data.identity_address, old_address);
  EXPECT_EQ(*data.identity_address, peer()->address());
  EXPECT_TRUE(peer()->identity_known());
  EXPECT_EQ(peer(), cache()->FindByAddress(*data.identity_address));

  // The old address should still map to |peer|.
  ASSERT_EQ(peer(), cache()->FindByAddress(old_address));
}

TEST_F(GAP_PeerCacheTest_BondingTest,
       StoreLowEnergyBondWithIrkIsAddedToResolvingList) {
  ASSERT_TRUE(NewPeer(kAddrLeRandom, true));
  ASSERT_FALSE(peer()->identity_known());

  sm::PairingData data;
  data.ltk = kLTK;
  data.identity_address = kAddrLeRandom;
  data.irk = sm::Key(sm::SecurityProperties(), common::RandomUInt128());

  EXPECT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  ASSERT_TRUE(peer()->le()->bonded());
  ASSERT_TRUE(peer()->identity_known());

  // Looking up the peer by RPA generated using the IRK should return the same
  // peer.
  DeviceAddress rpa = sm::util::GenerateRpa(data.irk->value());
  EXPECT_EQ(peer(), cache()->FindByAddress(rpa));
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreBrEdrBondWithUnknownAddress) {
  ASSERT_EQ(nullptr, cache()->FindByAddress(kAddrBrEdr));
  EXPECT_FALSE(cache()->StoreBrEdrBond(kAddrBrEdr, kBrEdrKey));
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreBrEdrBond) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_EQ(peer(), cache()->FindByAddress(kAddrBrEdr));
  ASSERT_TRUE(peer()->temporary());
  ASSERT_FALSE(peer()->bonded());
  ASSERT_TRUE(peer()->bredr());
  ASSERT_FALSE(peer()->bredr()->bonded());

  EXPECT_TRUE(cache()->StoreBrEdrBond(kAddrBrEdr, kBrEdrKey));

  EXPECT_FALSE(peer()->temporary());
  EXPECT_TRUE(peer()->bonded());
  EXPECT_TRUE(peer()->bredr()->bonded());
  EXPECT_TRUE(peer()->bredr()->link_key());
  EXPECT_EQ(kBrEdrKey, *peer()->bredr()->link_key());
}

TEST_F(GAP_PeerCacheTest_BondingTest, StoreBondsForBothTech) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_EQ(peer(), cache()->FindByAddress(kAddrBrEdr));
  ASSERT_TRUE(peer()->temporary());
  ASSERT_FALSE(peer()->bonded());

  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
  ASSERT_EQ(TechnologyType::kDualMode, peer()->technology());

  // Without Secure Connections cross-transport key generation, bonding on one
  // technology does not bond on the other.
  ASSERT_FALSE(kBrEdrKey.security().secure_connections());
  EXPECT_TRUE(cache()->StoreBrEdrBond(kAddrBrEdr, kBrEdrKey));
  EXPECT_TRUE(peer()->bonded());
  EXPECT_FALSE(peer()->le()->bonded());

  sm::PairingData data;
  data.ltk = kLTK;
  EXPECT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));

  EXPECT_FALSE(peer()->temporary());
  EXPECT_TRUE(peer()->bonded());
  EXPECT_TRUE(peer()->bredr()->bonded());
  EXPECT_TRUE(peer()->le()->bonded());
}

TEST_F(GAP_PeerCacheTest_BondingTest, ForgetUnknownPeer) {
  const PeerId id(0x9999);
  ASSERT_FALSE(cache()->FindById(id));
  EXPECT_FALSE(cache()->ForgetPeer(id));
  EXPECT_EQ(0, updated_callback_count());
}

TEST_F(GAP_PeerCacheTest_BondingTest, ForgetBrEdrPeer) {
  ASSERT_TRUE(NewPeer(kAddrBrEdr, true));
  ASSERT_TRUE(cache()->StoreBrEdrBond(kAddrBrEdr, kBrEdrKey));
  ASSERT_TRUE(peer()->bonded());
  ASSERT_FALSE(peer()->temporary());
  ASSERT_EQ(3, updated_callback_count());

  const PeerId id = peer()->identifier();
  EXPECT_TRUE(cache()->ForgetPeer(id));
  ASSERT_TRUE(cache()->FindById(id));
  EXPECT_FALSE(peer()->bonded());
  EXPECT_TRUE(peer()->temporary());
  EXPECT_EQ(4, updated_callback_count());
}

TEST_F(GAP_PeerCacheTest_BondingTest, ForgetLowEnergyPeerWithPublicAddress) {
  sm::PairingData data;
  data.ltk = kLTK;
  ASSERT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  ASSERT_EQ(DeviceAddress::Type::kLEPublic, peer()->address().type());
  ASSERT_TRUE(peer()->bonded());
  ASSERT_FALSE(peer()->temporary());
  ASSERT_EQ(2, updated_callback_count());

  const PeerId id = peer()->identifier();
  EXPECT_TRUE(cache()->ForgetPeer(id));
  ASSERT_TRUE(cache()->FindById(id));
  EXPECT_FALSE(peer()->bonded());
  EXPECT_TRUE(peer()->temporary());
  EXPECT_EQ(3, updated_callback_count());
}

TEST_F(GAP_PeerCacheTest_BondingTest, ForgetLowEnergyPeerWithIrk) {
  ASSERT_TRUE(NewPeer(kAddrLeRandom, true));

  sm::PairingData data;
  data.ltk = kLTK;
  data.identity_address = kAddrLeAlias;
  data.irk = sm::Key(sm::SecurityProperties(), common::RandomUInt128());
  DeviceAddress rpa = sm::util::GenerateRpa(data.irk->value());

  ASSERT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  ASSERT_TRUE(peer()->bonded());
  ASSERT_FALSE(peer()->temporary());
  ASSERT_TRUE(peer()->identity_known());
  ASSERT_EQ(peer(), cache()->FindByAddress(rpa));
  ASSERT_EQ(kAddrLeAlias, peer()->address());
  EXPECT_EQ(3, updated_callback_count());

  const PeerId id = peer()->identifier();
  EXPECT_TRUE(cache()->ForgetPeer(id));
  ASSERT_TRUE(cache()->FindById(id));
  EXPECT_FALSE(peer()->bonded());
  EXPECT_TRUE(peer()->temporary());
  EXPECT_FALSE(peer()->identity_known());
  EXPECT_FALSE(cache()->FindByAddress(rpa));

  // Don't expect the original random address to be restored.
  EXPECT_EQ(kAddrLeAlias, peer()->address());
  EXPECT_EQ(4, updated_callback_count());

  RunLoopFor(kCacheTimeout);
  EXPECT_FALSE(cache()->FindById(id));
}

// Test that a forgotten LE peer using address privacy eventually expires out
// of the cache.
TEST_F(GAP_PeerCacheTest_BondingTest,
       RemoveForgetedLePrivacyPeerAfterDisconnectionThenExpiry) {
  ASSERT_TRUE(NewPeer(kAddrLeRandom, true));

  sm::PairingData data;
  data.ltk = kLTK;
  data.identity_address = kAddrLeAlias;
  data.irk = sm::Key(sm::SecurityProperties(), common::RandomUInt128());

  ASSERT_TRUE(cache()->StoreLowEnergyBond(peer()->identifier(), data));
  peer()->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);
  ASSERT_TRUE(peer()->bonded());
  ASSERT_FALSE(peer()->temporary());

  const PeerId id = peer()->identifier();
  EXPECT_TRUE(cache()->ForgetPeer(id));
  ASSERT_TRUE(cache()->FindById(id));
  EXPECT_TRUE(peer()->temporary());

  RunLoopFor(kCacheTimeout);
  ASSERT_TRUE(cache()->FindById(id));

  peer()->MutLe().SetConnectionState(Peer::ConnectionState::kNotConnected);
  ASSERT_TRUE(cache()->FindById(id));
  EXPECT_TRUE(peer()->temporary());

  RunLoopFor(kCacheTimeout);
  EXPECT_FALSE(cache()->FindById(id));
}

// Fixture parameterized by peer address
class DualModeBondingTest
    : public GAP_PeerCacheTest_BondingTest,
      public ::testing::WithParamInterface<DeviceAddress> {};

TEST_P(DualModeBondingTest, AddBondedPeerSuccess) {
  PeerId kId(5);
  sm::PairingData data;
  data.ltk = kLTK;

  const DeviceAddress& address = GetParam();
  EXPECT_TRUE(cache()->AddBondedPeer(kId, address, data, kBrEdrKey));
  auto* peer = cache()->FindById(kId);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer, cache()->FindByAddress(kAddrLeAlias));
  EXPECT_EQ(peer, cache()->FindByAddress(kAddrBrEdr));
  EXPECT_EQ(kId, peer->identifier());
  EXPECT_EQ(address, peer->address());
  EXPECT_TRUE(peer->identity_known());
  EXPECT_TRUE(peer->bonded());
  ASSERT_TRUE(peer->le());
  EXPECT_TRUE(peer->le()->bonded());
  ASSERT_TRUE(peer->le()->bond_data());
  EXPECT_EQ(data, *peer->le()->bond_data());
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(peer->bredr()->bonded());
  ASSERT_TRUE(peer->bredr()->link_key());
  EXPECT_EQ(kBrEdrKey, *peer->bredr()->link_key());
  EXPECT_EQ(TechnologyType::kDualMode, peer->technology());

  // The "new bond" callback should not be called when restoring a previously
  // bonded peer.
  EXPECT_FALSE(bonded_callback_called());
}

TEST_P(DualModeBondingTest, ForgetPeer) {
  PeerId kId(5);
  sm::PairingData data;
  data.ltk = kLTK;

  const DeviceAddress& address = GetParam();
  EXPECT_TRUE(cache()->AddBondedPeer(kId, address, data, kBrEdrKey));
  auto* peer = cache()->FindById(kId);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bonded());
  EXPECT_EQ(5, updated_callback_count());

  // Forgetting a dual-mode peer should only invoke the update callback once.
  EXPECT_TRUE(cache()->ForgetPeer(kId));
  EXPECT_FALSE(peer->bonded());
  EXPECT_EQ(6, updated_callback_count());
}

// Test dual-mode character of peer using the same address of both types.
INSTANTIATE_TEST_SUITE_P(GAP_PeerCacheTest, DualModeBondingTest,
                         ::testing::Values(kAddrBrEdr, kAddrLeAlias));

template <const DeviceAddress* DevAddr>
class GAP_PeerCacheTest_UpdateCallbackTest : public GAP_PeerCacheTest {
 public:
  void SetUp() {
    was_called_ = false;
    ASSERT_TRUE(NewPeer(*DevAddr, true));
    cache()->set_peer_updated_callback(
        [this](const auto&) { was_called_ = true; });
    ir_.bd_addr = peer()->address().value();
    irr_.bd_addr = peer()->address().value();
    eirep_.bd_addr = peer()->address().value();
    eir_data().SetToZeros();
    EXPECT_FALSE(was_called_);
  }

 protected:
  hci::InquiryResult& ir() { return ir_; }
  hci::InquiryResultRSSI& irr() { return irr_; }
  hci::ExtendedInquiryResultEventParams& eirep() { return eirep_; }

  MutableBufferView eir_data() {
    return MutableBufferView(&eirep_.extended_inquiry_response,
                             sizeof(eirep_.extended_inquiry_response));
  }
  bool was_called() const { return was_called_; }
  void ClearWasCalled() { was_called_ = false; }

 private:
  bool was_called_;
  hci::InquiryResult ir_;
  hci::InquiryResultRSSI irr_;
  hci::ExtendedInquiryResultEventParams eirep_;
};

using GAP_PeerCacheTest_BrEdrUpdateCallbackTest =
    GAP_PeerCacheTest_UpdateCallbackTest<&kAddrBrEdr>;
using GAP_PeerCacheTest_LowEnergyUpdateCallbackTest =
    GAP_PeerCacheTest_UpdateCallbackTest<&kAddrLeAlias>;

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       ChangingLEConnectionStateTriggersUpdateCallback) {
  peer()->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);
  EXPECT_TRUE(was_called());
}

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       SetAdvertisingDataTriggersUpdateCallbackOnNameSet) {
  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
  EXPECT_TRUE(was_called());
  ASSERT_TRUE(peer()->name());
  EXPECT_EQ("Test", *peer()->name());
}

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       SetLowEnergyAdvertisingDataUpdateCallbackProvidesUpdatedPeer) {
  ASSERT_NE(peer()->rssi(), kTestRSSI);
  cache()->set_peer_updated_callback([&](const auto& updated_peer) {
    ASSERT_TRUE(updated_peer.le());
    EXPECT_TRUE(common::ContainersEqual(kAdvData,
                                        updated_peer.le()->advertising_data()));
    EXPECT_EQ(updated_peer.rssi(), kTestRSSI);
  });
  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
}

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       SetAdvertisingDataDoesNotTriggerUpdateCallbackOnSameName) {
  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
  ASSERT_TRUE(was_called());

  ClearWasCalled();
  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
  EXPECT_FALSE(was_called());
}

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       SetLowEnergyConnectionParamsDoesNotTriggerUpdateCallback) {
  peer()->MutLe().SetConnectionParameters({});
  EXPECT_FALSE(was_called());
}

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       SetLowEnergyPreferredConnectionParamsDoesNotTriggerUpdateCallback) {
  peer()->MutLe().SetPreferredConnectionParameters({});
  EXPECT_FALSE(was_called());
}

TEST_F(GAP_PeerCacheTest_LowEnergyUpdateCallbackTest,
       BecomingDualModeTriggersUpdateCallBack) {
  EXPECT_EQ(TechnologyType::kLowEnergy, peer()->technology());

  size_t call_count = 0;
  cache()->set_peer_updated_callback([&](const auto&) { ++call_count; });
  peer()->MutBrEdr();
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());
  EXPECT_EQ(call_count, 1U);

  // Calling MutBrEdr again on doesn't trigger additional callbacks.
  peer()->MutBrEdr();
  EXPECT_EQ(call_count, 1U);
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_EQ(call_count, 2U);
}

TEST_F(GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
       ChangingBrEdrConnectionStateTriggersUpdateCallback) {
  peer()->MutBrEdr().SetConnectionState(Peer::ConnectionState::kConnected);
  EXPECT_TRUE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromInquiryResultTriggersUpdateCallbackOnPeerClassSet) {
  ir().class_of_device = kTestDeviceClass;
  peer()->MutBrEdr().SetInquiryData(ir());
  EXPECT_TRUE(was_called());
}

TEST_F(GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
       SetBrEdrInquiryDataFromInquiryResultUpdateCallbackProvidesUpdatedPeer) {
  ir().class_of_device = kTestDeviceClass;
  cache()->set_peer_updated_callback([](const auto& updated_peer) {
    ASSERT_TRUE(updated_peer.bredr());
    ASSERT_TRUE(updated_peer.bredr()->device_class());
    EXPECT_EQ(common::DeviceClass::MajorClass(0x02),
              updated_peer.bredr()->device_class()->major_class());
  });
  peer()->MutBrEdr().SetInquiryData(ir());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromInquiryResultDoesNotTriggerUpdateCallbackOnSameDeviceClass) {
  ir().class_of_device = kTestDeviceClass;
  peer()->MutBrEdr().SetInquiryData(ir());
  ASSERT_TRUE(was_called());

  ClearWasCalled();
  peer()->MutBrEdr().SetInquiryData(ir());
  EXPECT_FALSE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromInquiryResultRSSITriggersUpdateCallbackOnDeviceClassSet) {
  irr().class_of_device = kTestDeviceClass;
  peer()->MutBrEdr().SetInquiryData(irr());
  EXPECT_TRUE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromInquiryResultRSSIUpdateCallbackProvidesUpdatedPeer) {
  irr().class_of_device = kTestDeviceClass;
  cache()->set_peer_updated_callback([](const auto& updated_peer) {
    ASSERT_TRUE(updated_peer.bredr()->device_class());
    EXPECT_EQ(common::DeviceClass::MajorClass(0x02),
              updated_peer.bredr()->device_class()->major_class());
  });
  peer()->MutBrEdr().SetInquiryData(irr());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromInquiryResultRSSIDoesNotTriggerUpdateCallbackOnSameDeviceClass) {
  irr().class_of_device = kTestDeviceClass;
  peer()->MutBrEdr().SetInquiryData(irr());
  ASSERT_TRUE(was_called());

  ClearWasCalled();
  peer()->MutBrEdr().SetInquiryData(irr());
  EXPECT_FALSE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromInquiryResultRSSIDoesNotTriggerUpdateCallbackOnRSSI) {
  irr().rssi = 1;
  peer()->MutBrEdr().SetInquiryData(irr());
  ASSERT_TRUE(was_called());  // Callback due to |class_of_device|.

  ClearWasCalled();
  irr().rssi = 20;
  peer()->MutBrEdr().SetInquiryData(irr());
  EXPECT_FALSE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsTriggersUpdateCallbackOnDeviceClassSet) {
  eirep().class_of_device = kTestDeviceClass;
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_TRUE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsTriggersUpdateCallbackOnNameSet) {
  peer()->MutBrEdr().SetInquiryData(eirep());
  ASSERT_TRUE(was_called());  // Callback due to |class_of_device|.

  ClearWasCalled();
  eir_data().Write(kEirData);
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_TRUE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsUpdateCallbackProvidesUpdatedPeer) {
  eirep().clock_offset = htole16(1);
  eirep().page_scan_repetition_mode = hci::PageScanRepetitionMode::kR1;
  eirep().rssi = kTestRSSI;
  eirep().class_of_device = kTestDeviceClass;
  eir_data().Write(kEirData);
  ASSERT_FALSE(peer()->name().has_value());
  ASSERT_EQ(peer()->rssi(), hci::kRSSIInvalid);
  cache()->set_peer_updated_callback([](const auto& updated_peer) {
    const auto& data = updated_peer.bredr();
    ASSERT_TRUE(data);
    ASSERT_TRUE(data->clock_offset().has_value());
    ASSERT_TRUE(data->page_scan_repetition_mode().has_value());
    ASSERT_TRUE(data->device_class().has_value());
    ASSERT_TRUE(updated_peer.name().has_value());

    EXPECT_EQ(*data->clock_offset(), 0x8001);
    EXPECT_EQ(*data->page_scan_repetition_mode(),
              hci::PageScanRepetitionMode::kR1);
    EXPECT_EQ(common::DeviceClass::MajorClass(0x02),
              updated_peer.bredr()->device_class()->major_class());
    EXPECT_EQ(updated_peer.rssi(), kTestRSSI);
    EXPECT_EQ(*updated_peer.name(), "Test");
  });
  peer()->MutBrEdr().SetInquiryData(eirep());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsGeneratesExactlyOneUpdateCallbackRegardlessOfNumberOfFieldsChanged) {
  eirep().clock_offset = htole16(1);
  eirep().page_scan_repetition_mode = hci::PageScanRepetitionMode::kR1;
  eirep().rssi = kTestRSSI;
  eirep().class_of_device = kTestDeviceClass;
  eir_data().Write(kEirData);

  size_t call_count = 0;
  cache()->set_peer_updated_callback([&](const auto&) { ++call_count; });
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_EQ(call_count, 1U);
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsDoesNotTriggerUpdateCallbackOnSamePeerClass) {
  eirep().class_of_device = kTestDeviceClass;
  peer()->MutBrEdr().SetInquiryData(eirep());
  ASSERT_TRUE(was_called());

  ClearWasCalled();
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_FALSE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsDoesNotTriggerUpdateCallbackOnSameName) {
  eir_data().Write(kEirData);
  peer()->MutBrEdr().SetInquiryData(eirep());
  ASSERT_TRUE(was_called());

  ClearWasCalled();
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_FALSE(was_called());
}

TEST_F(
    GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsDoesNotTriggerUpdateCallbackOnRSSI) {
  eirep().rssi = 1;
  peer()->MutBrEdr().SetInquiryData(eirep());
  ASSERT_TRUE(was_called());  // Callback due to |class_of_device|.

  ClearWasCalled();
  eirep().rssi = 20;
  peer()->MutBrEdr().SetInquiryData(eirep());
  EXPECT_FALSE(was_called());
}

TEST_F(GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
       SetNameTriggersUpdateCallback) {
  peer()->SetName("nombre");
  EXPECT_TRUE(was_called());
}

TEST_F(GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
       SetNameDoesNotTriggerUpdateCallbackOnSameName) {
  peer()->SetName("nombre");
  ASSERT_TRUE(was_called());

  bool was_called_again = false;
  cache()->set_peer_updated_callback(
      [&](const auto&) { was_called_again = true; });
  peer()->SetName("nombre");
  EXPECT_FALSE(was_called_again);
}

TEST_F(GAP_PeerCacheTest_BrEdrUpdateCallbackTest,
       BecomingDualModeTriggersUpdateCallBack) {
  EXPECT_EQ(TechnologyType::kClassic, peer()->technology());

  size_t call_count = 0;
  cache()->set_peer_updated_callback([&](const auto&) { ++call_count; });
  peer()->MutLe();
  EXPECT_EQ(TechnologyType::kDualMode, peer()->technology());
  EXPECT_EQ(call_count, 1U);

  // Calling MutLe again doesn't trigger additional callbacks.
  peer()->MutLe();
  EXPECT_EQ(call_count, 1U);
  peer()->MutLe().SetAdvertisingData(kTestRSSI, kAdvData);
  EXPECT_EQ(call_count, 2U);
}

class GAP_PeerCacheExpirationTest : public ::gtest::TestLoopFixture {
 public:
  void SetUp() {
    TestLoopFixture::SetUp();
    cache_.set_peer_removed_callback([this](PeerId) { peers_removed_++; });
    auto* peer = cache_.NewPeer(kAddrLeAlias, true /*connectable*/);
    ASSERT_TRUE(peer);
    ASSERT_TRUE(peer->temporary());
    peer_addr_ = peer->address();
    peer_addr_alias_ = kAddrBrEdr;
    peer_id_ = peer->identifier();
    peers_removed_ = 0;
  }

  void TearDown() {
    cache_.set_peer_removed_callback(nullptr);
    RunLoopUntilIdle();
    TestLoopFixture::TearDown();
  }

  Peer* GetDefaultPeer() { return cache_.FindById(peer_id_); }
  Peer* GetPeerById(PeerId id) { return cache_.FindById(id); }
  bool IsDefaultPeerAddressInCache() const {
    return cache_.FindByAddress(peer_addr_);
  }
  bool IsOtherTransportAddressInCache() const {
    return cache_.FindByAddress(peer_addr_alias_);
  }
  bool IsDefaultPeerPresent() { return GetDefaultPeer(); }
  Peer* NewPeer(const common::DeviceAddress& address, bool connectable) {
    return cache_.NewPeer(address, connectable);
  }
  int peers_removed() const { return peers_removed_; }

 private:
  PeerCache cache_;
  common::DeviceAddress peer_addr_;
  common::DeviceAddress peer_addr_alias_;
  PeerId peer_id_;
  int peers_removed_;
};

TEST_F(GAP_PeerCacheExpirationTest, TemporaryDiesSixtySecondsAfterBirth) {
  RunLoopFor(kCacheTimeout);
  EXPECT_FALSE(IsDefaultPeerPresent());
  EXPECT_EQ(1, peers_removed());
}

TEST_F(GAP_PeerCacheExpirationTest, TemporaryLivesForSixtySecondsAfterBirth) {
  RunLoopFor(kCacheTimeout - zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
  EXPECT_EQ(0, peers_removed());
}

TEST_F(GAP_PeerCacheExpirationTest,
       TemporaryLivesForSixtySecondsSinceLastSeen) {
  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());

  // Tickle peer, and verify it sticks around for another cache timeout.
  GetDefaultPeer()->SetName("nombre");
  RunLoopFor(kCacheTimeout - zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest, TemporaryDiesSixtySecondsAfterLastSeen) {
  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());

  // Tickle peer, and verify it expires after cache timeout.
  GetDefaultPeer()->SetName("nombre");
  RunLoopFor(kCacheTimeout);
  EXPECT_FALSE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest, CanMakeNonTemporaryJustBeforeSixtySeconds) {
  // At last possible moment, make peer non-temporary,
  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutLe().SetConnectionState(
      Peer::ConnectionState::kConnected);
  ASSERT_FALSE(GetDefaultPeer()->temporary());

  // Verify that the peer survives.
  RunLoopFor(kCacheTimeout * 10);
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest,
       LEConnectedPeerLivesMuchMoreThanSixtySeconds) {
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutLe().SetConnectionState(
      Peer::ConnectionState::kConnected);
  RunLoopFor(kCacheTimeout * 10);
  ASSERT_TRUE(IsDefaultPeerPresent());
  EXPECT_FALSE(GetDefaultPeer()->temporary());
}

TEST_F(GAP_PeerCacheExpirationTest,
       BREDRConnectedPeerLivesMuchMoreThanSixtySeconds) {
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutBrEdr().SetConnectionState(
      Peer::ConnectionState::kConnected);
  RunLoopFor(kCacheTimeout * 10);
  ASSERT_TRUE(IsDefaultPeerPresent());
  EXPECT_FALSE(GetDefaultPeer()->temporary());
}

TEST_F(GAP_PeerCacheExpirationTest, LePeerBecomesNonTemporaryWhenConnecting) {
  ASSERT_TRUE(IsDefaultPeerPresent());
  ASSERT_EQ(kAddrLeAlias, GetDefaultPeer()->address());
  ASSERT_TRUE(GetDefaultPeer()->temporary());

  GetDefaultPeer()->MutLe().SetConnectionState(
      Peer::ConnectionState::kInitializing);
  EXPECT_FALSE(GetDefaultPeer()->temporary());

  RunLoopFor(kCacheTimeout);
  ASSERT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest,
       LEPublicPeerRemainsNonTemporaryOnDisconnect) {
  ASSERT_TRUE(IsDefaultPeerPresent());
  ASSERT_EQ(kAddrLeAlias, GetDefaultPeer()->address());
  GetDefaultPeer()->MutLe().SetConnectionState(
      Peer::ConnectionState::kConnected);
  ASSERT_FALSE(GetDefaultPeer()->temporary());

  RunLoopFor(zx::sec(61));
  ASSERT_TRUE(IsDefaultPeerPresent());
  ASSERT_TRUE(GetDefaultPeer()->identity_known());

  GetDefaultPeer()->MutLe().SetConnectionState(
      Peer::ConnectionState::kNotConnected);
  EXPECT_FALSE(GetDefaultPeer()->temporary());

  RunLoopFor(kCacheTimeout);
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest, LERandomPeerBecomesTemporaryOnDisconnect) {
  // Create our Peer, and get it into the kConnected state.
  PeerId custom_peer_id;
  {
    auto* custom_peer = NewPeer(kAddrLeRandom, true);
    ASSERT_TRUE(custom_peer);
    ASSERT_TRUE(custom_peer->temporary());
    ASSERT_FALSE(custom_peer->identity_known());
    custom_peer_id = custom_peer->identifier();

    custom_peer->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);
    ASSERT_FALSE(custom_peer->temporary());
    ASSERT_FALSE(custom_peer->identity_known());
  }

  // Verify that the connected peer does not expire out of the cache.
  // Then disconnect the peer, in preparation for the next stage of our test.
  {
    EXPECT_EQ(0, peers_removed());
    RunLoopFor(zx::sec(61));
    EXPECT_EQ(1, peers_removed());  // Default peer timed out.
    auto* custom_peer = GetPeerById(custom_peer_id);
    ASSERT_TRUE(custom_peer);
    ASSERT_FALSE(custom_peer->identity_known());

    custom_peer->MutLe().SetConnectionState(
        Peer::ConnectionState::kNotConnected);
    EXPECT_TRUE(custom_peer->temporary());
    EXPECT_FALSE(custom_peer->identity_known());
  }

  // Verify that the disconnected peer expires out of the cache.
  RunLoopFor(zx::sec(61));
  EXPECT_FALSE(GetPeerById(custom_peer_id));
  EXPECT_EQ(2, peers_removed());
}

TEST_F(GAP_PeerCacheExpirationTest, BrEdrPeerRemainsNonTemporaryOnDisconnect) {
  // Create our Peer, and get it into the kConnected state.
  PeerId custom_peer_id;
  {
    auto* custom_peer = NewPeer(kAddrLePublic, true);
    ASSERT_TRUE(custom_peer);
    custom_peer->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);
    custom_peer_id = custom_peer->identifier();
  }

  // Verify that the connected peer does not expire out of the cache.
  // Then disconnect the peer, in preparation for the next stage of our test.
  {
    EXPECT_EQ(0, peers_removed());
    RunLoopFor(kCacheTimeout * 10);
    EXPECT_EQ(1, peers_removed());  // Default peer timed out.
    auto* custom_peer = GetPeerById(custom_peer_id);
    ASSERT_TRUE(custom_peer);
    ASSERT_TRUE(custom_peer->identity_known());
    EXPECT_FALSE(custom_peer->temporary());

    custom_peer->MutLe().SetConnectionState(
        Peer::ConnectionState::kNotConnected);
    ASSERT_TRUE(GetPeerById(custom_peer_id));
    EXPECT_FALSE(custom_peer->temporary());
  }

  // Verify that the disconnected peer does _not_ expire out of the cache.
  // We expect the peer to remain, because BrEdr peers are non-temporary
  // even when disconnected.
  RunLoopFor(kCacheTimeout);
  EXPECT_TRUE(GetPeerById(custom_peer_id));
  EXPECT_EQ(1, peers_removed());
}

TEST_F(GAP_PeerCacheExpirationTest, ExpirationUpdatesAddressMap) {
  ASSERT_TRUE(IsDefaultPeerAddressInCache());
  ASSERT_TRUE(IsOtherTransportAddressInCache());
  RunLoopFor(kCacheTimeout);
  EXPECT_FALSE(IsDefaultPeerAddressInCache());
  EXPECT_FALSE(IsOtherTransportAddressInCache());
}

TEST_F(GAP_PeerCacheExpirationTest, SetAdvertisingDataUpdatesExpiration) {
  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutLe().SetAdvertisingData(kTestRSSI,
                                               StaticByteBuffer<1>{});
  RunLoopFor(zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest,
       SetBrEdrInquiryDataFromInquiryResultUpdatesExpiration) {
  hci::InquiryResult ir;
  ASSERT_TRUE(IsDefaultPeerPresent());
  ir.bd_addr = GetDefaultPeer()->address().value();

  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutBrEdr().SetInquiryData(ir);

  RunLoopFor(zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest,
       SetBrEdrInquiryDataFromInquiryResultRSSIUpdatesExpiration) {
  hci::InquiryResultRSSI irr;
  ASSERT_TRUE(IsDefaultPeerPresent());
  irr.bd_addr = GetDefaultPeer()->address().value();

  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutBrEdr().SetInquiryData(irr);

  RunLoopFor(zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(
    GAP_PeerCacheExpirationTest,
    SetBrEdrInquiryDataFromExtendedInquiryResultEventParamsUpdatesExpiration) {
  hci::ExtendedInquiryResultEventParams eirep;
  ASSERT_TRUE(IsDefaultPeerPresent());
  eirep.bd_addr = GetDefaultPeer()->address().value();

  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->MutBrEdr().SetInquiryData(eirep);

  RunLoopFor(zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
}

TEST_F(GAP_PeerCacheExpirationTest, SetNameUpdatesExpiration) {
  RunLoopFor(kCacheTimeout - zx::msec(1));
  ASSERT_TRUE(IsDefaultPeerPresent());
  GetDefaultPeer()->SetName({});
  RunLoopFor(zx::msec(1));
  EXPECT_TRUE(IsDefaultPeerPresent());
}

}  // namespace
}  // namespace gap
}  // namespace bt
