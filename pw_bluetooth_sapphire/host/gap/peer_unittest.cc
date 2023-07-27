// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/manufacturer_names.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/inspect_util.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::gap {
namespace {

#ifndef NINSPECT
using namespace inspect::testing;
using bt::testing::GetInspectValue;
using bt::testing::ReadInspect;

constexpr uint16_t kManufacturer = 0x0001;
constexpr uint16_t kSubversion = 0x0002;
#endif  // NINSPECT

const StaticByteBuffer kAdvData(0x05,  // Length
                                0x09,  // AD type: Complete Local Name
                                'T', 'e', 's', 't');
const StaticByteBuffer kInvalidAdvData{
    // 32 bit service UUIDs are supposed to be 4 bytes, but the value in this TLV field is only 3
    // bytes long, hence the AdvertisingData is not valid.
    0x04, static_cast<uint8_t>(DataType::kComplete32BitServiceUuids), 0x01, 0x02, 0x03,
};

const bt::sm::LTK kLTK;

const DeviceAddress kAddrLePublic(DeviceAddress::Type::kLEPublic, {1, 2, 3, 4, 5, 6});
const DeviceAddress kAddrLeRandom(DeviceAddress::Type::kLERandom, {1, 2, 3, 4, 5, 6});
const DeviceAddress kAddrBrEdr(DeviceAddress::Type::kBREDR, {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA});
// LE Public Device Address that has the same value as a BR/EDR BD_ADDR, e.g. on
// a dual-mode device.
const DeviceAddress kAddrLeAlias(DeviceAddress::Type::kLEPublic,
                                 {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA});

const bt::sm::LTK kSecureBrEdrKey(sm::SecurityProperties(/*encrypted=*/true, /*authenticated=*/true,
                                                         /*secure_connections=*/true,
                                                         sm::kMaxEncryptionKeySize),
                                  hci_spec::LinkKey(UInt128{4}, 5, 6));

class PeerTest : public ::gtest::TestLoopFixture {
 public:
  PeerTest() = default;

  void SetUp() override {
    TestLoopFixture::SetUp();
    // Set up a default peer.
    SetUpPeer(/*address=*/kAddrLePublic, /*connectable=*/true);
  }

  void TearDown() override {
    peer_.reset();
    TestLoopFixture::TearDown();
  }

 protected:
  // Can be used to override or reset the default peer. Resets metrics to prevent interference
  // between peers (e.g. by metrics updated in construction).
  void SetUpPeer(const DeviceAddress& address, bool connectable) {
    address_ = address;
    peer_ = std::make_unique<Peer>(fit::bind_member<&PeerTest::NotifyListenersCallback>(this),
                                   fit::bind_member<&PeerTest::UpdateExpiryCallback>(this),
                                   fit::bind_member<&PeerTest::DualModeCallback>(this),
                                   fit::bind_member<&PeerTest::StoreLowEnergyBondCallback>(this),
                                   PeerId(1), address_, connectable, &metrics_);
    peer_->AttachInspect(peer_inspector_.GetRoot());
    // Reset metrics as they should only apply to the new peer under test.
    metrics_.AttachInspect(metrics_inspector_.GetRoot());
  }
  Peer& peer() { return *peer_; }

#ifndef NINSPECT
  inspect::Hierarchy ReadPeerInspect() { return ReadInspect(peer_inspector_); }

  std::string InspectLowEnergyConnectionState() {
    std::optional<std::string> val = GetInspectValue<inspect::StringPropertyValue>(
        peer_inspector_, {"peer", "le_data", Peer::LowEnergyData::kInspectConnectionStateName});
    BT_ASSERT(val);
    return *val;
  }

  int64_t InspectAdvertisingDataParseFailureCount() {
    std::optional<int64_t> val = GetInspectValue<inspect::IntPropertyValue>(
        peer_inspector_,
        {"peer", "le_data", Peer::LowEnergyData::kInspectAdvertisingDataParseFailureCountName});
    BT_ASSERT(val);
    return *val;
  }

  std::string InspectLastAdvertisingDataParseFailure() {
    std::optional<std::string> val = GetInspectValue<inspect::StringPropertyValue>(
        peer_inspector_,
        {"peer", "le_data", Peer::LowEnergyData::kInspectLastAdvertisingDataParseFailureName});
    BT_ASSERT(val);
    return *val;
  }

  uint64_t MetricsLowEnergyConnections() {
    std::optional<uint64_t> val = GetInspectValue<inspect::UintPropertyValue>(
        metrics_inspector_, {"metrics", "le", "connection_events"});
    BT_ASSERT(val);
    return *val;
  }

  uint64_t MetricsLowEnergyDisconnections() {
    std::optional<uint64_t> val = GetInspectValue<inspect::UintPropertyValue>(
        metrics_inspector_, {"metrics", "le", "disconnection_events"});
    BT_ASSERT(val);
    return *val;
  }

  std::string InspectBrEdrConnectionState() {
    std::optional<std::string> val = GetInspectValue<inspect::StringPropertyValue>(
        peer_inspector_, {"peer", "bredr_data", Peer::BrEdrData::kInspectConnectionStateName});
    BT_ASSERT(val);
    return *val;
  }

  uint64_t MetricsBrEdrConnections() {
    std::optional<uint64_t> val = GetInspectValue<inspect::UintPropertyValue>(
        metrics_inspector_, {"metrics", "bredr", "connection_events"});
    BT_ASSERT(val);
    return *val;
  }

  uint64_t MetricsBrEdrDisconnections() {
    std::optional<uint64_t> val = GetInspectValue<inspect::UintPropertyValue>(
        metrics_inspector_, {"metrics", "bredr", "disconnection_events"});
    BT_ASSERT(val);
    return *val;
  }
#endif  // NINSPECT

  void set_notify_listeners_cb(Peer::NotifyListenersCallback cb) {
    notify_listeners_cb_ = std::move(cb);
  }
  void set_update_expiry_cb(Peer::PeerCallback cb) { update_expiry_cb_ = std::move(cb); }
  void set_dual_mode_cb(Peer::PeerCallback cb) { dual_mode_cb_ = std::move(cb); }
  void set_store_le_bond_cb(Peer::StoreLowEnergyBondCallback cb) {
    store_le_bond_cb_ = std::move(cb);
  }

 private:
  void NotifyListenersCallback(const Peer& peer, Peer::NotifyListenersChange change) {
    if (notify_listeners_cb_) {
      notify_listeners_cb_(peer, change);
    }
  }

  void UpdateExpiryCallback(const Peer& peer) {
    if (update_expiry_cb_) {
      update_expiry_cb_(peer);
    }
  }

  void DualModeCallback(const Peer& peer) {
    if (dual_mode_cb_) {
      dual_mode_cb_(peer);
    }
  }

  bool StoreLowEnergyBondCallback(const sm::PairingData& data) {
    if (store_le_bond_cb_) {
      return store_le_bond_cb_(data);
    }
    return false;
  }

  std::unique_ptr<Peer> peer_;
  DeviceAddress address_;
  Peer::NotifyListenersCallback notify_listeners_cb_;
  Peer::PeerCallback update_expiry_cb_;
  Peer::PeerCallback dual_mode_cb_;
  Peer::StoreLowEnergyBondCallback store_le_bond_cb_;
  inspect::Inspector metrics_inspector_;
  PeerMetrics metrics_;
  inspect::Inspector peer_inspector_;
};

class PeerDeathTest : public PeerTest {};

#ifndef NINSPECT
TEST_F(PeerTest, InspectHierarchy) {
  peer().set_version(pw::bluetooth::emboss::CoreSpecificationVersion::V5_0, kManufacturer,
                     kSubversion);

  peer().RegisterName("Sapphire💖", Peer::NameSource::kGenericAccessService);

  peer().MutLe();
  ASSERT_TRUE(peer().le().has_value());

  peer().MutLe().SetFeatures(hci_spec::LESupportedFeatures{0x0000000000000001});

  peer().MutBrEdr().AddService(UUID(uint16_t{0x110b}));

  // clang-format off
  auto bredr_data_matcher = AllOf(
    NodeMatches(AllOf(
      NameMatches(Peer::BrEdrData::kInspectNodeName),
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::BrEdrData::kInspectConnectionStateName,
                 Peer::ConnectionStateToString(peer().bredr()->connection_state())),
        BoolIs(Peer::BrEdrData::kInspectLinkKeyName, peer().bredr()->bonded()),
        StringIs(Peer::BrEdrData::kInspectServicesName, "{ 0000110b-0000-1000-8000-00805f9b34fb }")
        )))));

  auto le_data_matcher = AllOf(
    NodeMatches(AllOf(
      NameMatches(Peer::LowEnergyData::kInspectNodeName),
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::LowEnergyData::kInspectConnectionStateName,
                 Peer::ConnectionStateToString(peer().le()->connection_state())),
        IntIs(Peer::LowEnergyData::kInspectAdvertisingDataParseFailureCountName, 0),
        StringIs(Peer::LowEnergyData::kInspectLastAdvertisingDataParseFailureName, ""),
        BoolIs(Peer::LowEnergyData::kInspectBondDataName, peer().le()->bonded()),
        StringIs(Peer::LowEnergyData::kInspectFeaturesName, "0x0000000000000001")
        )))));

  auto peer_matcher = AllOf(
    NodeMatches(
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::kInspectPeerIdName, peer().identifier().ToString()),
        StringIs(Peer::kInspectPeerNameName, peer().name().value() + " [source: " + Peer::NameSourceToString(Peer::NameSource::kGenericAccessService) + "]"),
        StringIs(Peer::kInspectTechnologyName, TechnologyTypeToString(peer().technology())),
        StringIs(Peer::kInspectAddressName, peer().address().ToString()),
        BoolIs(Peer::kInspectConnectableName, peer().connectable()),
        BoolIs(Peer::kInspectTemporaryName, peer().temporary()),
        StringIs(Peer::kInspectFeaturesName, peer().features().ToString()),
        StringIs(Peer::kInspectVersionName, hci_spec::HCIVersionToString(peer().version().value())),
        StringIs(Peer::kInspectManufacturerName, GetManufacturerName(kManufacturer))
        ))),
    ChildrenMatch(UnorderedElementsAre(bredr_data_matcher, le_data_matcher)));
  // clang-format on
  inspect::Hierarchy hierarchy = ReadPeerInspect();
  EXPECT_THAT(hierarchy, AllOf(ChildrenMatch(UnorderedElementsAre(peer_matcher))));
}
#endif  // NINSPECT

TEST_F(PeerTest, BrEdrDataAddServiceNotifiesListeners) {
  // Initialize BrEdrData.
  peer().MutBrEdr();
  ASSERT_TRUE(peer().bredr()->services().empty());

  bool listener_notified = false;
  set_notify_listeners_cb([&](auto&, Peer::NotifyListenersChange change) {
    listener_notified = true;
    // Non-bonded peer should not update bond
    EXPECT_EQ(Peer::NotifyListenersChange::kBondNotUpdated, change);
  });

  constexpr UUID kServiceUuid;
  peer().MutBrEdr().AddService(kServiceUuid);
  EXPECT_TRUE(listener_notified);
  EXPECT_EQ(1u, peer().bredr()->services().count(kServiceUuid));

  // De-duplicate subsequent additions of the same service.
  listener_notified = false;
  peer().MutBrEdr().AddService(kServiceUuid);
  EXPECT_FALSE(listener_notified);
}

TEST_F(PeerTest, BrEdrDataAddServiceOnBondedPeerNotifiesListenersToUpdateBond) {
  // Initialize BrEdrData.
  peer().MutBrEdr().SetBondData({});
  ASSERT_TRUE(peer().bredr()->services().empty());

  bool listener_notified = false;
  set_notify_listeners_cb([&](auto&, Peer::NotifyListenersChange change) {
    listener_notified = true;
    // Bonded peer should update bond
    EXPECT_EQ(Peer::NotifyListenersChange::kBondUpdated, change);
  });

  peer().MutBrEdr().AddService(UUID());
  EXPECT_TRUE(listener_notified);
}

TEST_F(PeerTest, LowEnergyDataSetAdvDataWithInvalidUtf8NameDoesNotUpdatePeerName) {
  peer().MutLe();  // Initialize LowEnergyData.
  ASSERT_FALSE(peer().name().has_value());

  bool listener_notified = false;
  set_notify_listeners_cb([&](auto&, Peer::NotifyListenersChange) { listener_notified = true; });

  const StaticByteBuffer kAdvData(0x05,  // Length
                                  0x09,  // AD type: Complete Local Name
                                  'T', 'e', 's',
                                  0xFF  // 0xFF should not appear in a valid UTF-8 string
  );

  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time());
  EXPECT_TRUE(listener_notified);  // Fresh AD still results in an update
  EXPECT_FALSE(peer().name().has_value());
}

TEST_F(PeerTest, BrEdrDataSetEirDataWithInvalidUtf8NameDoesNotUpdatePeerName) {
  peer().MutBrEdr();  // Initialize BrEdrData.
  ASSERT_FALSE(peer().name().has_value());

  bool listener_notified = false;
  set_notify_listeners_cb([&](auto&, Peer::NotifyListenersChange) { listener_notified = true; });

  const StaticByteBuffer kEirData(0x05,  // Length
                                  0x09,  // AD type: Complete Local Name
                                  'T', 'e', 's',
                                  0xFF  // 0xFF should not appear in a valid UTF-8 string
  );
  hci_spec::ExtendedInquiryResultEventParams eirep;
  eirep.num_responses = 1;
  eirep.bd_addr = peer().address().value();
  MutableBufferView(eirep.extended_inquiry_response, sizeof(eirep.extended_inquiry_response))
      .Write(kEirData);

  peer().MutBrEdr().SetInquiryData(eirep);
  EXPECT_TRUE(listener_notified);  // Fresh EIR data still results in an update
  EXPECT_FALSE(peer().name().has_value());
}

TEST_F(PeerTest, RegisterNameWithInvalidUtf8NameDoesNotUpdatePeerName) {
  ASSERT_FALSE(peer().name().has_value());

  bool listener_notified = false;
  set_notify_listeners_cb([&](auto&, Peer::NotifyListenersChange) { listener_notified = true; });

  const std::string kName = "Tes\xFF\x01";  // 0xFF should not appear in a valid UTF-8 string
  peer().RegisterName(kName);
  EXPECT_FALSE(listener_notified);
  EXPECT_FALSE(peer().name().has_value());
}

TEST_F(PeerTest, LowEnergyAdvertisingDataTimestamp) {
  EXPECT_FALSE(peer().MutLe().parsed_advertising_data_timestamp());
  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time(1));
  ASSERT_TRUE(peer().MutLe().parsed_advertising_data_timestamp());
  EXPECT_EQ(peer().MutLe().parsed_advertising_data_timestamp().value(), zx::time(1));

  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time(2));
  ASSERT_TRUE(peer().MutLe().parsed_advertising_data_timestamp());
  EXPECT_EQ(peer().MutLe().parsed_advertising_data_timestamp().value(), zx::time(2));

  // SetAdvertisingData with data that fails to parse should not update the advertising data
  // timestamp.
  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kInvalidAdvData, zx::time(3));
  ASSERT_TRUE(peer().MutLe().parsed_advertising_data_timestamp());
  EXPECT_EQ(peer().MutLe().parsed_advertising_data_timestamp().value(), zx::time(2));
}

TEST_F(PeerTest, SettingLowEnergyAdvertisingDataUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time(1));
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, RegisteringLowEnergyInitializingConnectionUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  Peer::InitializingConnectionToken token = peer().MutLe().RegisterInitializingConnection();
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, SettingLowEnergyBondDataUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  sm::PairingData data;
  data.peer_ltk = kLTK;
  data.local_ltk = kLTK;
  peer().MutLe().SetBondData(data);
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, RegisteringBrEdrInitializingConnectionUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  Peer::InitializingConnectionToken token = peer().MutBrEdr().RegisterInitializingConnection();
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, SettingInquiryDataUpdatesLastUpdated) {
  SetUpPeer(/*address=*/kAddrLeAlias, /*connectable=*/true);
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  StaticPacket<pw::bluetooth::emboss::InquiryResultWriter> ir;
  ir.view().bd_addr().CopyFrom(kAddrLeAlias.value().view());
  peer().MutBrEdr().SetInquiryData(ir.view());
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, SettingBrEdrBondDataUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  peer().MutBrEdr().SetBondData(kSecureBrEdrKey);
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, SettingAddingBrEdrServiceUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  peer().MutBrEdr().AddService(UUID(uint16_t{0x110b}));
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, RegisteringNameUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  peer().RegisterName("name");
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, RegisterAndUnregisterTwoLowEnergyConnections) {
  SetUpPeer(/*address=*/kAddrLeRandom, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::ConnectionToken> token_0 = peer().MutLe().RegisterConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
  EXPECT_EQ(MetricsLowEnergyConnections(), 1u);
#endif  // NINSPECT

  std::optional<Peer::ConnectionToken> token_1 = peer().MutLe().RegisterConnection();
  // The second connection should not update expiry or notify.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
  // Although the second connection does not change the high-level connection state, we track it in
  // metrics to support multiple connections to the same peer.
  EXPECT_EQ(MetricsLowEnergyConnections(), 2u);
  EXPECT_EQ(MetricsLowEnergyDisconnections(), 0u);
#endif  // NINSPECT

  token_0.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
  EXPECT_EQ(MetricsLowEnergyDisconnections(), 1u);
#endif  // NINSPECT

  token_1.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
  EXPECT_EQ(MetricsLowEnergyDisconnections(), 2u);
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterLowEnergyConnectionsWhenIdentityKnown) {
  EXPECT_TRUE(peer().identity_known());
  std::optional<Peer::ConnectionToken> token = peer().MutLe().RegisterConnection();
  EXPECT_FALSE(peer().temporary());
  token.reset();
  // The peer's identity is known, so it should stay non-temporary upon disconnection.
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterInitializingLowEnergyConnectionsWhenIdentityKnown) {
  EXPECT_TRUE(peer().identity_known());
  std::optional<Peer::InitializingConnectionToken> token =
      peer().MutLe().RegisterInitializingConnection();
  EXPECT_FALSE(peer().temporary());
  token.reset();
  // The peer's identity is known, so it should stay non-temporary upon disconnection.
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterLowEnergyConnectionDuringInitializingConnection) {
  SetUpPeer(/*address=*/kAddrLeRandom, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::InitializingConnectionToken> init_token =
      peer().MutLe().RegisterInitializingConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the initializing connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  std::optional<Peer::ConnectionToken> conn_token = peer().MutLe().RegisterConnection();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 4);
  EXPECT_EQ(notify_count, 4);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  init_token.reset();
  EXPECT_EQ(update_expiry_count, 5);
  EXPECT_EQ(notify_count, 5);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterInitializingLowEnergyConnectionDuringConnection) {
  SetUpPeer(/*address=*/kAddrLeRandom, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::ConnectionToken> conn_token = peer().MutLe().RegisterConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the initializing connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  std::optional<Peer::InitializingConnectionToken> init_token =
      peer().MutLe().RegisterInitializingConnection();
  // Initializing connections should not affect the expiry or notify listeners for peers that are
  // already connected.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  init_token.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterTwoLowEnergyInitializingConnections) {
  SetUpPeer(/*address=*/kAddrLeRandom, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::InitializingConnectionToken> token_0 =
      peer().MutLe().RegisterInitializingConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the initializing connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  std::optional<Peer::InitializingConnectionToken> token_1 =
      peer().MutLe().RegisterInitializingConnection();
  // The second initializing connection should not update expiry or notify.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  token_0.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT
  token_1.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  // The peer's identity is not known, so it should become temporary upon disconnection.
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, MovingLowEnergyConnectionTokenWorksAsExpected) {
  std::optional<Peer::ConnectionToken> token_0 = peer().MutLe().RegisterConnection();
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);

  std::optional<Peer::ConnectionToken> token_1 = std::move(token_0);
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);

  token_0.reset();
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);

  token_1.reset();
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
}

TEST_F(PeerTest, RegisterNamesWithVariousSources) {
  ASSERT_FALSE(peer().name().has_value());
  ASSERT_TRUE(peer().RegisterName("test", Peer::kAdvertisingDataComplete));

  // Test that name with lower source priority does not replace stored name with higher priority.
  ASSERT_FALSE(peer().RegisterName("test", Peer::kUnknown));

  // Test that name with higher source priority replaces stored name with lower priority.
  ASSERT_TRUE(peer().RegisterName("test", Peer::kGenericAccessService));

  // Test that stored name is not replaced with an identical name from an identical source.
  ASSERT_FALSE(peer().RegisterName("test", Peer::kGenericAccessService));

  // Test that stored name is replaced by a different name from the same source.
  ASSERT_TRUE(peer().RegisterName("different_name", Peer::kGenericAccessService));
}

TEST_F(PeerTest, SetValidAdvertisingData) {
  constexpr const char* kLocalName = "Test";
  StaticByteBuffer raw_data{
      // Length - Type - Value formatted Local name
      0x05,          static_cast<uint8_t>(DataType::kCompleteLocalName),
      kLocalName[0], kLocalName[1],
      kLocalName[2], kLocalName[3],
  };
  peer().MutLe().SetAdvertisingData(/*rssi=*/32, raw_data, zx::time());
  // Setting an AdvertisingData with a local name field should update the peer's local name.
  ASSERT_TRUE(peer().name().has_value());
  EXPECT_EQ(kLocalName, peer().name().value());
  EXPECT_EQ(Peer::NameSource::kAdvertisingDataComplete, peer().name_source());
#ifndef NINSPECT
  EXPECT_EQ(0, InspectAdvertisingDataParseFailureCount());
  EXPECT_EQ("", InspectLastAdvertisingDataParseFailure());
#endif  // NINSPECT
}

TEST_F(PeerTest, SetShortenedLocalName) {
  constexpr const char* kLocalName = "Test";
  StaticByteBuffer raw_data{
      // Length - Type - Value formatted Local name
      0x05,          static_cast<uint8_t>(DataType::kShortenedLocalName),
      kLocalName[0], kLocalName[1],
      kLocalName[2], kLocalName[3],
  };
  peer().MutLe().SetAdvertisingData(/*rssi=*/32, raw_data, zx::time());
  ASSERT_TRUE(peer().name().has_value());
  EXPECT_EQ(kLocalName, peer().name().value());
  EXPECT_EQ(Peer::NameSource::kAdvertisingDataShortened, peer().name_source());
}

TEST_F(PeerTest, SetInvalidAdvertisingData) {
  peer().MutLe().SetAdvertisingData(/*rssi=*/32, kInvalidAdvData, zx::time());

#ifndef NINSPECT
  EXPECT_EQ(1, InspectAdvertisingDataParseFailureCount());
  EXPECT_EQ(AdvertisingData::ParseErrorToString(AdvertisingData::ParseError::kUuidsMalformed),
            InspectLastAdvertisingDataParseFailure());
#endif  // NINSPECT
}

TEST_F(PeerDeathTest, RegisterTwoBrEdrConnectionsAsserts) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);
  std::optional<Peer::ConnectionToken> token_0 = peer().MutBrEdr().RegisterConnection();
  ASSERT_DEATH_IF_SUPPORTED(
      { std::optional<Peer::ConnectionToken> token_1 = peer().MutBrEdr().RegisterConnection(); },
      ".*already registered.*");
}

TEST_F(PeerTest, RegisterAndUnregisterInitializingBrEdrConnectionLeavesPeerTemporary) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);
  EXPECT_TRUE(peer().identity_known());
  std::optional<Peer::InitializingConnectionToken> token =
      peer().MutBrEdr().RegisterInitializingConnection();
  EXPECT_FALSE(peer().temporary());
  token.reset();
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterBrEdrConnectionWithoutBonding) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::ConnectionToken> conn_token = peer().MutBrEdr().RegisterConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the initializing connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  // BR/EDR peers should become non-temporary after disconnecting if not bonded.
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterBrEdrConnectionWithBonding) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::ConnectionToken> conn_token = peer().MutBrEdr().RegisterConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the initializing connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  peer().MutBrEdr().SetBondData(kSecureBrEdrKey);
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 3);

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 4);
  // Bonded BR/EDR peers should remain non-temporary after disconnecting.
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterAndUnregisterBrEdrConnectionDuringInitializingConnection) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::InitializingConnectionToken> init_token =
      peer().MutBrEdr().RegisterInitializingConnection();
  // Expiry is updated for state change + becoming non-temporary.
  EXPECT_EQ(update_expiry_count, 2);
  // 1 notification for becoming non-temporary.
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  // The connection state should not change when registering a connection because the peer is still
  // initializing.
  std::optional<Peer::ConnectionToken> conn_token = peer().MutBrEdr().RegisterConnection();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  init_token.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 1);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, RegisterBrEdrConnectionDuringInitializingConnectionAndThenCompleteInitialization) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::InitializingConnectionToken> init_token =
      peer().MutBrEdr().RegisterInitializingConnection();
  // Expiry is updated for state change + becoming non-temporary.
  EXPECT_EQ(update_expiry_count, 2);
  // 1 notification for becoming non-temporary.
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  // The connection state should not change when registering a connection because the peer is still
  // initializing.
  std::optional<Peer::ConnectionToken> conn_token = peer().MutBrEdr().RegisterConnection();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  // When initialization completes, the connection state should become kConnected.
  init_token.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 4);
  EXPECT_EQ(notify_count, 3);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerDeathTest, RegisterInitializingBrEdrConnectionDuringConnectionAsserts) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::ConnectionToken> conn_token = peer().MutBrEdr().RegisterConnection();
  // A notification and expiry update are sent when the peer becomes non-temporary, and a second
  // notification and update expiry are sent because the initializing connection is registered.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kConnected);
#ifndef NINSPECT
  EXPECT_EQ(InspectBrEdrConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
#endif  // NINSPECT

  // Registering an initializing connection when the peer is already connected should assert.
  ASSERT_DEATH_IF_SUPPORTED(
      {
        Peer::InitializingConnectionToken init_token =
            peer().MutBrEdr().RegisterInitializingConnection();
      },
      ".*connected.*");
}

TEST_F(PeerTest, RegisterAndUnregisterTwoBrEdrInitializingConnections) {
  SetUpPeer(/*address=*/kAddrBrEdr, /*connectable=*/true);

  int update_expiry_count = 0;
  set_update_expiry_cb([&](const Peer&) { update_expiry_count++; });
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  std::optional<Peer::InitializingConnectionToken> token_0 =
      peer().MutBrEdr().RegisterInitializingConnection();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  std::optional<std::string> inspect_conn_state = InspectBrEdrConnectionState();
  ASSERT_TRUE(inspect_conn_state);
  EXPECT_EQ(inspect_conn_state.value(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  std::optional<Peer::InitializingConnectionToken> token_1 =
      peer().MutBrEdr().RegisterInitializingConnection();
  // The second initializing connection should not update expiry or notify.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  inspect_conn_state = InspectBrEdrConnectionState();
  ASSERT_TRUE(inspect_conn_state);
  EXPECT_EQ(inspect_conn_state.value(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  token_0.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 1);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kInitializing);
#ifndef NINSPECT
  inspect_conn_state = InspectBrEdrConnectionState();
  ASSERT_TRUE(inspect_conn_state);
  EXPECT_EQ(inspect_conn_state.value(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
#endif  // NINSPECT

  token_1.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 1);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().bredr()->connection_state(), Peer::ConnectionState::kNotConnected);
#ifndef NINSPECT
  inspect_conn_state = InspectBrEdrConnectionState();
  ASSERT_TRUE(inspect_conn_state);
  EXPECT_EQ(inspect_conn_state.value(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
#endif  // NINSPECT
}

TEST_F(PeerTest, SettingLeAdvertisingDataOfBondedPeerDoesNotUpdateName) {
  peer().RegisterName("alice");
  sm::PairingData data;
  data.peer_ltk = kLTK;
  data.local_ltk = kLTK;
  peer().MutLe().SetBondData(data);

  const StaticByteBuffer kAdvData(0x08,  // Length
                                  0x09,  // AD type: Complete Local Name
                                  'M', 'a', 'l', 'l', 'o', 'r', 'y');
  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time(0));

  ASSERT_TRUE(peer().name().has_value());
  EXPECT_EQ(peer().name().value(), "alice");
}

TEST_F(PeerTest, SettingInquiryDataOfBondedPeerDoesNotUpdateName) {
  peer().RegisterName("alice");
  peer().MutBrEdr().SetBondData(kLTK);

  const StaticByteBuffer kEirData(0x08,  // Length
                                  0x09,  // AD type: Complete Local Name
                                  'M', 'a', 'l', 'l', 'o', 'r', 'y');
  hci_spec::ExtendedInquiryResultEventParams eirep;
  eirep.num_responses = 1;
  eirep.bd_addr = peer().address().value();
  MutableBufferView(eirep.extended_inquiry_response, sizeof(eirep.extended_inquiry_response))
      .Write(kEirData);
  peer().MutBrEdr().SetInquiryData(eirep);

  ASSERT_TRUE(peer().name().has_value());
  EXPECT_EQ(peer().name().value(), "alice");
}

TEST_F(PeerTest, LowEnergyStoreBondCallsCallback) {
  int cb_count = 0;
  set_store_le_bond_cb([&cb_count](const sm::PairingData& data) {
    cb_count++;
    return true;
  });

  sm::PairingData data;
  data.peer_ltk = kLTK;
  data.local_ltk = kLTK;
  EXPECT_TRUE(peer().MutLe().StoreBond(data));
  EXPECT_EQ(cb_count, 1);
}

}  // namespace
}  // namespace bt::gap
