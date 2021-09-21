// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"

#include <lib/async/cpp/executor.h>
#include <lib/fpromise/single_threaded_executor.h>
#include <lib/inspect/testing/cpp/inspect.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/manufacturer_names.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"

namespace bt::gap {
namespace {

using namespace inspect::testing;

constexpr uint16_t kManufacturer = 0x0001;
constexpr uint16_t kSubversion = 0x0002;

const auto kAdvData = StaticByteBuffer(0x05,  // Length
                                       0x09,  // AD type: Complete Local Name
                                       'T', 'e', 's', 't');

const bt::sm::LTK kLTK;

const DeviceAddress kAddrLePublic(DeviceAddress::Type::kLEPublic, {1, 2, 3, 4, 5, 6});
const DeviceAddress kAddrLeRandom(DeviceAddress::Type::kLERandom, {1, 2, 3, 4, 5, 6});
const DeviceAddress kAddrBrEdr(DeviceAddress::Type::kBREDR, {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA});
// LE Public Device Address that has the same value as a BR/EDR BD_ADDR, e.g. on
// a dual-mode device.
const DeviceAddress kAddrLeAlias(DeviceAddress::Type::kLEPublic,
                                 {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA});

const bt::sm::LTK kSecureBrEdrKey(sm::SecurityProperties(true /*encrypted*/, true /*authenticated*/,
                                                         true /*secure_connections*/,
                                                         sm::kMaxEncryptionKeySize),
                                  hci::LinkKey(UInt128{4}, 5, 6));

inspect::Hierarchy ReadInspect(inspect::Inspector& inspector) {
  fpromise::single_threaded_executor executor;
  fpromise::result<inspect::Hierarchy> hierarchy;
  executor.schedule_task(inspect::ReadFromInspector(inspector).then(
      [&](fpromise::result<inspect::Hierarchy>& res) { hierarchy = std::move(res); }));
  executor.run();
  ZX_ASSERT(hierarchy.is_ok());
  return hierarchy.take_value();
}

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
    peer_ = std::make_unique<Peer>(fit::bind_member(this, &PeerTest::NotifyListenersCallback),
                                   fit::bind_member(this, &PeerTest::UpdateExpiryCallback),
                                   fit::bind_member(this, &PeerTest::DualModeCallback), PeerId(1),
                                   address_, connectable, &metrics_);
    peer_->AttachInspect(peer_inspector_.GetRoot());
    // Reset metrics as they should only apply to the new peer under test.
    metrics_.AttachInspect(metrics_inspector_.GetRoot());
  }
  Peer& peer() { return *peer_; }

  inspect::Hierarchy ReadPeerInspect() { return ReadInspect(peer_inspector_); }

  template <class PropertyValue>
  std::optional<std::remove_reference_t<decltype(std::declval<PropertyValue>().value())>>
  InspectPropertyValueAtPath(inspect::Inspector& inspector, const std::vector<std::string>& path,
                             const std::string& property) {
    inspect::Hierarchy hierarchy = ReadInspect(inspector);
    auto node = hierarchy.GetByPath(path);
    if (!node) {
      return std::nullopt;
    }
    const PropertyValue* prop_value = node->node().get_property<PropertyValue>(property);
    if (!prop_value) {
      return std::nullopt;
    }
    return prop_value->value();
  }

  std::string InspectLowEnergyConnectionState() {
    std::optional<std::string> val = InspectPropertyValueAtPath<inspect::StringPropertyValue>(
        peer_inspector_, {"peer", "le_data"}, Peer::LowEnergyData::kInspectConnectionStateName);
    ZX_ASSERT(val);
    return *val;
  }

  uint64_t MetricsLowEnergyConnections() {
    std::optional<uint64_t> val = InspectPropertyValueAtPath<inspect::UintPropertyValue>(
        metrics_inspector_, {"metrics", "le"}, "connection_events");
    ZX_ASSERT(val);
    return *val;
  }

  uint64_t MetricsLowEnergyDisconnections() {
    std::optional<uint64_t> val = InspectPropertyValueAtPath<inspect::UintPropertyValue>(
        metrics_inspector_, {"metrics", "le"}, "disconnection_events");
    ZX_ASSERT(val);
    return *val;
  }

  void set_notify_listeners_cb(Peer::NotifyListenersCallback cb) {
    notify_listeners_cb_ = std::move(cb);
  }
  void set_update_expiry_cb(Peer::PeerCallback cb) { update_expiry_cb_ = std::move(cb); }
  void set_dual_mode_cb(Peer::PeerCallback cb) { dual_mode_cb_ = std::move(cb); }

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

  std::unique_ptr<Peer> peer_;
  DeviceAddress address_;
  Peer::NotifyListenersCallback notify_listeners_cb_;
  Peer::PeerCallback update_expiry_cb_;
  Peer::PeerCallback dual_mode_cb_;
  inspect::Inspector metrics_inspector_;
  PeerMetrics metrics_;
  inspect::Inspector peer_inspector_;
};

TEST_F(PeerTest, InspectHierarchy) {
  peer().set_version(hci::HCIVersion::k5_0, kManufacturer, kSubversion);

  peer().MutLe();
  ASSERT_TRUE(peer().le().has_value());

  peer().MutLe().SetFeatures(hci::LESupportedFeatures{0x0000000000000001});

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
        BoolIs(Peer::LowEnergyData::kInspectBondDataName, peer().le()->bonded()),
        StringIs(Peer::LowEnergyData::kInspectFeaturesName, "0x0000000000000001")
        )))));

  auto peer_matcher = AllOf(
    NodeMatches(
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::kInspectPeerIdName, peer().identifier().ToString()),
        StringIs(Peer::kInspectTechnologyName, TechnologyTypeToString(peer().technology())),
        StringIs(Peer::kInspectAddressName, peer().address().ToString()),
        BoolIs(Peer::kInspectConnectableName, peer().connectable()),
        BoolIs(Peer::kInspectTemporaryName, peer().temporary()),
        StringIs(Peer::kInspectFeaturesName, peer().features().ToString()),
        StringIs(Peer::kInspectVersionName, hci::HCIVersionToString(peer().version().value())),
        StringIs(Peer::kInspectManufacturerName, GetManufacturerName(kManufacturer))
        ))),
    ChildrenMatch(UnorderedElementsAre(bredr_data_matcher, le_data_matcher)));
  // clang-format on
  inspect::Hierarchy hierarchy = ReadPeerInspect();
  EXPECT_THAT(hierarchy, AllOf(ChildrenMatch(UnorderedElementsAre(peer_matcher))));
}

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
  hci::ExtendedInquiryResultEventParams eirep;
  eirep.num_responses = 1;
  eirep.bd_addr = peer().address().value();
  MutableBufferView(eirep.extended_inquiry_response, sizeof(eirep.extended_inquiry_response))
      .Write(kEirData);

  peer().MutBrEdr().SetInquiryData(eirep);
  EXPECT_TRUE(listener_notified);  // Fresh EIR data still results in an update
  EXPECT_FALSE(peer().name().has_value());
}

TEST_F(PeerTest, SetNameWithInvalidUtf8NameDoesNotUpdatePeerName) {
  ASSERT_FALSE(peer().name().has_value());

  bool listener_notified = false;
  set_notify_listeners_cb([&](auto&, Peer::NotifyListenersChange) { listener_notified = true; });

  const std::string kName = "Tes\xFF\x01";  // 0xFF should not appear in a valid UTF-8 string
  peer().SetName(kName);
  EXPECT_FALSE(listener_notified);
  EXPECT_FALSE(peer().name().has_value());
}

TEST_F(PeerTest, LowEnergyAdvertisingDataTimestamp) {
  EXPECT_FALSE(peer().MutLe().advertising_data_timestamp());
  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time(1));
  ASSERT_TRUE(peer().MutLe().advertising_data_timestamp());
  EXPECT_EQ(peer().MutLe().advertising_data_timestamp().value(), zx::time(1));

  peer().MutLe().SetAdvertisingData(/*rssi=*/0, kAdvData, zx::time(2));
  ASSERT_TRUE(peer().MutLe().advertising_data_timestamp());
  EXPECT_EQ(peer().MutLe().advertising_data_timestamp().value(), zx::time(2));
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

TEST_F(PeerTest, SettingBrEdrConnectionStateUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  peer().MutBrEdr().SetConnectionState(Peer::ConnectionState::kInitializing);
  EXPECT_EQ(peer().last_updated(), zx::time(2));
  EXPECT_GE(notify_count, 1);
}

TEST_F(PeerTest, SettingBrEdrConnectionStateUpdatesTemporary) {
  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) { notify_count++; });

  peer().MutBrEdr().SetConnectionState(Peer::ConnectionState::kInitializing);
  ASSERT_FALSE(peer().temporary());
  // Notifications: one for non-temporary.
  EXPECT_EQ(notify_count, 1);

  peer().MutBrEdr().SetConnectionState(Peer::ConnectionState::kNotConnected);
  ASSERT_TRUE(peer().temporary());
  EXPECT_EQ(notify_count, 1);

  peer().MutBrEdr().SetConnectionState(Peer::ConnectionState::kInitializing);
  ASSERT_FALSE(peer().temporary());
  // +1 notification (non-temporary)
  EXPECT_EQ(notify_count, 2);

  peer().MutBrEdr().SetConnectionState(Peer::ConnectionState::kConnected);
  peer().MutBrEdr().SetBondData(kSecureBrEdrKey);
  ASSERT_FALSE(peer().temporary());
  // +2 notification (connected, bonded)
  EXPECT_EQ(notify_count, 4);

  peer().MutBrEdr().SetConnectionState(Peer::ConnectionState::kNotConnected);
  ASSERT_FALSE(peer().temporary());
  // +1 notification (connection state)
  EXPECT_EQ(notify_count, 5);
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
  hci::InquiryResult ir;
  ir.bd_addr = kAddrLeAlias.value();
  peer().MutBrEdr().SetInquiryData(ir);
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

TEST_F(PeerTest, SettingNameUpdatesLastUpdated) {
  EXPECT_EQ(peer().last_updated(), zx::time(0));

  int notify_count = 0;
  set_notify_listeners_cb([&](const Peer&, Peer::NotifyListenersChange) {
    EXPECT_EQ(peer().last_updated(), zx::time(2));
    notify_count++;
  });

  RunLoopFor(zx::duration(2));
  peer().SetName("name");
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
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
  EXPECT_EQ(MetricsLowEnergyConnections(), 1u);

  std::optional<Peer::ConnectionToken> token_1 = peer().MutLe().RegisterConnection();
  // The second connection should not update expiry or notify.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
  // Although the second connection does not change the high-level connection state, we track it in
  // metrics to support multiple connections to the same peer.
  EXPECT_EQ(MetricsLowEnergyConnections(), 2u);
  EXPECT_EQ(MetricsLowEnergyDisconnections(), 0u);

  token_0.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));
  EXPECT_EQ(MetricsLowEnergyDisconnections(), 1u);

  token_1.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
  EXPECT_EQ(MetricsLowEnergyDisconnections(), 2u);
}

TEST_F(PeerTest, RegisterAndUnregisterLowEnergyConnectionsWhenIdentityKnown) {
  EXPECT_TRUE(peer().identity_known());
  std::optional<Peer::ConnectionToken> token = peer().MutLe().RegisterConnection();
  EXPECT_FALSE(peer().temporary());
  token.reset();
  // The peer's identity is known, so it should stay non-temporary upon disconnection.
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
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
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
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
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));

  std::optional<Peer::ConnectionToken> conn_token = peer().MutLe().RegisterConnection();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 4);
  EXPECT_EQ(notify_count, 4);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));

  init_token.reset();
  EXPECT_EQ(update_expiry_count, 5);
  EXPECT_EQ(notify_count, 5);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
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
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));

  std::optional<Peer::InitializingConnectionToken> init_token =
      peer().MutLe().RegisterInitializingConnection();
  // Initializing connections should not affect the expiry or notify listeners for peers that are
  // already connected.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));

  init_token.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kConnected));

  conn_token.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
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
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));

  std::optional<Peer::InitializingConnectionToken> token_1 =
      peer().MutLe().RegisterInitializingConnection();
  // The second initializing connection should not update expiry or notify.
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));

  token_0.reset();
  EXPECT_EQ(update_expiry_count, 2);
  EXPECT_EQ(notify_count, 2);
  EXPECT_FALSE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kInitializing);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kInitializing));
  token_1.reset();
  EXPECT_EQ(update_expiry_count, 3);
  EXPECT_EQ(notify_count, 3);
  // The peer's identity is not known, so it should become temporary upon disconnection.
  EXPECT_TRUE(peer().temporary());
  EXPECT_EQ(peer().le()->connection_state(), Peer::ConnectionState::kNotConnected);
  EXPECT_EQ(InspectLowEnergyConnectionState(),
            Peer::ConnectionStateToString(Peer::ConnectionState::kNotConnected));
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

}  // namespace
}  // namespace bt::gap
