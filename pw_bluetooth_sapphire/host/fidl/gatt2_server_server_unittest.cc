// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt2_server_server.h"

#include <fuchsia/bluetooth/gatt2/cpp/fidl_test_base.h>
#include <zircon/status.h>

#include <algorithm>
#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/gatt2/cpp/fidl.h"
#include "lib/fidl/cpp/interface_handle.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer_test.h"

namespace bthost {
namespace {

namespace fbg = fuchsia::bluetooth::gatt2;
namespace fbt = fuchsia::bluetooth;

const std::vector<uint8_t> kBuffer = {0x00, 0x01, 0x02};

fbg::Characteristic BuildSimpleCharacteristic(uint64_t handle) {
  fbg::Characteristic chrc;
  fbg::Handle chrc_handle{handle};
  chrc.set_handle(chrc_handle);
  chrc.set_type(fbt::Uuid{{6}});
  chrc.set_properties(fbg::CharacteristicPropertyBits::READ);
  fbg::AttributePermissions permissions;
  fbg::SecurityRequirements security;
  security.set_encryption_required(true);
  permissions.set_read(std::move(security));
  chrc.set_permissions(std::move(permissions));
  return chrc;
}

fbg::ServiceInfo BuildSimpleService(uint64_t svc_handle = 1, bt::UInt128 svc_type = {5},
                                    uint64_t chrc_handle = 2) {
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{svc_handle});
  svc_info.set_type(fbt::Uuid{svc_type});

  std::vector<fbg::Characteristic> chrcs;
  chrcs.push_back(BuildSimpleCharacteristic(chrc_handle));
  svc_info.set_characteristics(std::move(chrcs));

  return svc_info;
}

class MockLocalService final : private ServerBase<fbg::LocalService> {
 public:
  using ReadValueFunction =
      fit::function<void(fbt::PeerId, fbg::Handle, int32_t, ReadValueCallback)>;
  using WriteValueFunction =
      fit::function<void(fbg::LocalServiceWriteValueRequest, WriteValueCallback)>;
  using CccFunction = fit::function<void(fbt::PeerId, fbg::Handle, bool, bool,
                                         CharacteristicConfigurationCallback)>;

  MockLocalService(fidl::InterfaceRequest<fbg::LocalService> request)
      : ServerBase<fbg::LocalService>(this, std::move(request)) {
    set_error_handler([this](zx_status_t status) { error_ = status; });
  }

  void set_read_value_function(ReadValueFunction func) { read_value_func_ = std::move(func); }
  void set_write_value_function(WriteValueFunction func) { write_value_func_ = std::move(func); }
  void set_ccc_function(CccFunction func) { ccc_func_ = std::move(func); }

  int credits() const { return credits_; }

  const std::vector<uint8_t>& credit_log() const { return credits_log_; }

  void NotifyValue(fbg::ValueChangedParameters update) {
    credits_--;
    binding()->events().OnNotifyValue(std::move(update));
  }

  void IndicateValue(fbg::ValueChangedParameters update, zx::eventpair confirmation) {
    credits_--;
    binding()->events().OnIndicateValue(std::move(update), std::move(confirmation));
  }

  std::optional<zx_status_t> error() const { return error_; }

 private:
  // fbg::LocalService overrides:
  void CharacteristicConfiguration(fbt::PeerId peer_id, fbg::Handle handle, bool notify,
                                   bool indicate,
                                   CharacteristicConfigurationCallback callback) override {
    if (ccc_func_) {
      ccc_func_(peer_id, handle, notify, indicate, std::move((callback)));
    }
  }
  void ReadValue(fbt::PeerId peer_id, fbg::Handle handle, int32_t offset,
                 ReadValueCallback callback) override {
    if (read_value_func_) {
      read_value_func_(peer_id, handle, offset, std::move(callback));
    }
  }
  void WriteValue(fbg::LocalServiceWriteValueRequest req, WriteValueCallback callback) override {
    if (write_value_func_) {
      write_value_func_(std::move(req), std::move(callback));
    }
  }
  void PeerUpdate(fbg::LocalServicePeerUpdateRequest req, PeerUpdateCallback callback) override {}
  void ValueChangedCredit(uint8_t additional_credit) override {
    credits_ += additional_credit;
    credits_log_.push_back(additional_credit);
  }

  ReadValueFunction read_value_func_;
  WriteValueFunction write_value_func_;
  CccFunction ccc_func_;
  // Use signed integer because in tests it is possible to have negative credits.
  int credits_ = fbg::INITIAL_VALUE_CHANGED_CREDITS;
  std::vector<uint8_t> credits_log_;
  std::optional<zx_status_t> error_;
};

class Gatt2ServerServerTest : public bt::gatt::testing::FakeLayerTest {
 public:
  ~Gatt2ServerServerTest() override = default;

  Gatt2ServerServerTest() {}

  void SetUp() override {
    // Create and connect to production GATT2 Server implementation
    fidl::InterfaceHandle<fbg::Server> server_handle;
    server_ = std::make_unique<Gatt2ServerServer>(gatt()->AsWeakPtr(), server_handle.NewRequest());
    server_ptr_ = server_handle.Bind();
  }

  void TearDown() override {}

 protected:
  fbg::ServerPtr& server_ptr() { return server_ptr_; }

  void DestroyServer() { server_.reset(); }

 private:
  // Proxy interface to the GATT2 Server implementation.
  fbg::ServerPtr server_ptr_;
  // Raw GATT2 Server implementation
  std::unique_ptr<Gatt2ServerServer> server_;
};

TEST_F(Gatt2ServerServerTest, PublishAndRemoveServiceWithTwoCharacteristicsSuccess) {
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{1});
  bt::UInt128 svc_type = {5};
  svc_info.set_type(fbt::Uuid{svc_type});

  std::vector<fbg::Characteristic> characteristics;
  characteristics.push_back(BuildSimpleCharacteristic(/*handle=*/2));
  characteristics.push_back(BuildSimpleCharacteristic(/*handle=*/3));
  svc_info.set_characteristics(std::move(characteristics));

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_0;
  fidl::InterfaceRequest<fbg::LocalService> request = local_service_handle_0.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle_0),
                               std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  bt::gatt::Service* service = gatt()->local_services().begin()->second.service.get();
  EXPECT_EQ(service->type(), svc_type);
  EXPECT_TRUE(service->primary());
  ASSERT_EQ(service->characteristics().size(), 2u);
  EXPECT_EQ(service->characteristics()[0]->id(), 2u);
  EXPECT_EQ(service->characteristics()[1]->id(), 3u);

  request.Close(ZX_ERR_PEER_CLOSED);
  RunLoopUntilIdle();
  EXPECT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, DestroyingServerUnregistersService) {
  bt::UInt128 svc_type = {5};
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, svc_type);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  fidl::InterfaceRequest<fbg::LocalService> request = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  EXPECT_EQ(gatt()->local_services().begin()->second.service->type().value(), svc_type);
  DestroyServer();
  EXPECT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceWithoutHandleFails) {
  fbg::ServiceInfo svc_info = BuildSimpleService();
  svc_info.clear_handle();

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  auto request = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    ASSERT_TRUE(res.is_err());
    EXPECT_EQ(res.err(), fbg::PublishServiceError::INVALID_SERVICE_HANDLE);
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  EXPECT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceWithoutTypeFails) {
  fbg::ServiceInfo svc_info = BuildSimpleService();
  svc_info.clear_type();

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  auto request = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    ASSERT_TRUE(res.is_err());
    EXPECT_EQ(res.err(), fbg::PublishServiceError::INVALID_UUID);
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  EXPECT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceWithoutCharacteristicsFails) {
  fbg::ServiceInfo svc_info = BuildSimpleService();
  svc_info.clear_characteristics();

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  auto request = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    ASSERT_TRUE(res.is_err());
    EXPECT_EQ(res.err(), fbg::PublishServiceError::INVALID_CHARACTERISTICS);
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  EXPECT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceWithReusedHandleFails) {
  fbg::ServiceInfo svc_info_0 = BuildSimpleService();

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_0;
  auto request_0 = local_service_handle_0.NewRequest();

  int cb_count_0 = 0;
  auto cb_0 = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count_0++;
  };
  server_ptr()->PublishService(std::move(svc_info_0), std::move(local_service_handle_0),
                               std::move(cb_0));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count_0, 1);
  EXPECT_EQ(gatt()->local_services().size(), 1u);

  fbg::ServiceInfo svc_info_1 = BuildSimpleService();

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_1;
  auto request_1 = local_service_handle_1.NewRequest();

  int cb_count_1 = 0;
  auto cb_1 = [&](fbg::Server_PublishService_Result res) {
    ASSERT_TRUE(res.is_err());
    EXPECT_EQ(res.err(), fbg::PublishServiceError::INVALID_SERVICE_HANDLE);
    cb_count_1++;
  };
  server_ptr()->PublishService(std::move(svc_info_1), std::move(local_service_handle_1),
                               std::move(cb_1));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count_1, 1);
  EXPECT_EQ(gatt()->local_services().size(), 1u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceWithReusedHandleAcrossTwoGattServersSuccceeds) {
  // Both services are identical.
  fbg::ServiceInfo svc_info_0 = BuildSimpleService();
  fbg::ServiceInfo svc_info_1 = BuildSimpleService();

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_0;
  auto request_0 = local_service_handle_0.NewRequest();

  int cb_count_0 = 0;
  auto cb_0 = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count_0++;
  };
  server_ptr()->PublishService(std::move(svc_info_0), std::move(local_service_handle_0),
                               std::move(cb_0));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count_0, 1);
  EXPECT_EQ(gatt()->local_services().size(), 1u);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_1;
  auto request_1 = local_service_handle_1.NewRequest();

  int cb_count_1 = 0;
  auto cb_1 = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count_1++;
  };

  // Create and connect to a second GATT Server implementation
  fidl::InterfaceHandle<fbg::Server> server_handle_1;
  auto server_1 =
      std::make_unique<Gatt2ServerServer>(gatt()->AsWeakPtr(), server_handle_1.NewRequest());
  fidl::InterfacePtr<fuchsia::bluetooth::gatt2::Server> server_ptr_1 = server_handle_1.Bind();
  // Publish an identical service.
  server_ptr_1->PublishService(std::move(svc_info_1), std::move(local_service_handle_1),
                               std::move(cb_1));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count_1, 1);
  ASSERT_EQ(gatt()->local_services().size(), 2u);
}

TEST_F(Gatt2ServerServerTest, PublishTwoServicesSuccess) {
  bt::UInt128 svc_type_0 = {5};
  fbg::ServiceInfo svc_info_0 = BuildSimpleService(/*svc_handle=*/1, svc_type_0);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_0;
  fidl::InterfaceRequest<fbg::LocalService> request_0 = local_service_handle_0.NewRequest();

  int cb_count_0 = 0;
  auto cb_0 = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count_0++;
  };
  server_ptr()->PublishService(std::move(svc_info_0), std::move(local_service_handle_0),
                               std::move(cb_0));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count_0, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  bt::gatt::Service* service = gatt()->local_services().begin()->second.service.get();
  EXPECT_EQ(service->type(), svc_type_0);

  bt::UInt128 svc_type_1 = {6};
  fbg::ServiceInfo svc_info_1 = BuildSimpleService(/*svc_handle=*/9, svc_type_1);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_1;
  fidl::InterfaceRequest<fbg::LocalService> request_1 = local_service_handle_1.NewRequest();

  int cb_count_1 = 0;
  auto cb_1 = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count_1++;
  };
  server_ptr()->PublishService(std::move(svc_info_1), std::move(local_service_handle_1),
                               std::move(cb_1));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count_1, 1);
  ASSERT_EQ(gatt()->local_services().size(), 2u);
  auto svc_iter = gatt()->local_services().begin();
  EXPECT_EQ(svc_iter->second.service->type(), svc_type_0);
  svc_iter++;
  EXPECT_EQ(svc_iter->second.service->type(), svc_type_1);

  request_0.Close(ZX_ERR_PEER_CLOSED);
  request_1.Close(ZX_ERR_PEER_CLOSED);
  RunLoopUntilIdle();
  EXPECT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, PublishSecondaryService) {
  bt::UInt128 svc_type = {5};
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, svc_type);
  svc_info.set_kind(fbg::ServiceKind::SECONDARY);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  fidl::InterfaceRequest<fbg::LocalService> request = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();
  EXPECT_EQ(svc_iter->second.service->type(), svc_type);
  EXPECT_FALSE(svc_iter->second.service->primary());
}

TEST_F(Gatt2ServerServerTest, ReadSuccess) {
  uint64_t svc_handle = 1;
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{svc_handle});
  svc_info.set_type(fbt::Uuid{{5}});
  fbg::Characteristic chrc;
  fbg::Handle chrc_handle{2};
  chrc.set_handle(chrc_handle);
  chrc.set_type(fbt::Uuid{{0}});
  chrc.set_properties(fbg::CharacteristicPropertyBits::READ);
  chrc.set_permissions(fbg::AttributePermissions());
  std::vector<fbg::Characteristic> chrcs;
  chrcs.push_back(std::move(chrc));
  svc_info.set_characteristics(std::move(chrcs));

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc(local_service_handle.NewRequest());

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();
  ASSERT_EQ(svc_iter->second.service->characteristics().size(), 1u);

  bt::gatt::IdType svc_id = svc_iter->first;
  bt::gatt::IdType chrc_id = svc_iter->second.service->characteristics()[0]->id();
  bt::PeerId peer_id(99);

  int read_value_count = 0;
  local_svc.set_read_value_function([&](fbt::PeerId cb_peer_id, fbg::Handle handle, int32_t offset,
                                        fbg::LocalService::ReadValueCallback callback) {
    read_value_count++;
    EXPECT_EQ(peer_id.value(), cb_peer_id.value);
    EXPECT_EQ(handle.value, chrc_handle.value);
    EXPECT_EQ(offset, 3);
    callback(fpromise::ok(kBuffer));
  });

  int read_responder_count = 0;
  auto read_responder = [&](fit::result<bt::att::ErrorCode> status, const bt::ByteBuffer& value) {
    read_responder_count++;
    EXPECT_TRUE(status.is_ok());
    EXPECT_THAT(value, ::testing::ElementsAreArray(kBuffer));
  };

  svc_iter->second.read_handler(peer_id, svc_id, chrc_id, /*offset=*/3, read_responder);
  RunLoopUntilIdle();
  EXPECT_EQ(read_value_count, 1);
  EXPECT_EQ(read_responder_count, 1);
}

TEST_F(Gatt2ServerServerTest, ReadErrorResponse) {
  uint64_t svc_handle = 1;
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{svc_handle});
  svc_info.set_type(fbt::Uuid{{5}});
  fbg::Characteristic chrc;
  fbg::Handle chrc_handle{2};
  chrc.set_handle(chrc_handle);
  chrc.set_type(fbt::Uuid{{0}});
  chrc.set_properties(fbg::CharacteristicPropertyBits::READ);
  chrc.set_permissions(fbg::AttributePermissions());
  std::vector<fbg::Characteristic> chrcs;
  chrcs.push_back(std::move(chrc));
  svc_info.set_characteristics(std::move(chrcs));

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc(local_service_handle.NewRequest());

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();
  ASSERT_EQ(svc_iter->second.service->characteristics().size(), 1u);

  bt::gatt::IdType svc_id = svc_iter->first;
  bt::gatt::IdType chrc_id = svc_iter->second.service->characteristics()[0]->id();

  int read_value_count = 0;
  local_svc.set_read_value_function([&](fbt::PeerId cb_peer_id, fbg::Handle handle, int32_t offset,
                                        fbg::LocalService::ReadValueCallback callback) {
    read_value_count++;
    callback(fpromise::error(fbg::Error::READ_NOT_PERMITTED));
  });

  int read_responder_count = 0;
  auto read_responder = [&](fit::result<bt::att::ErrorCode> status, const bt::ByteBuffer& value) {
    read_responder_count++;
    ASSERT_TRUE(status.is_error());
    EXPECT_EQ(status.error_value(), bt::att::ErrorCode::kReadNotPermitted);
    EXPECT_EQ(value.size(), 0u);
  };

  svc_iter->second.read_handler(bt::PeerId(42), svc_id, chrc_id, /*offset=*/0, read_responder);
  RunLoopUntilIdle();
  EXPECT_EQ(read_value_count, 1);
  EXPECT_EQ(read_responder_count, 1);
}

TEST_F(Gatt2ServerServerTest, WriteSuccess) {
  uint64_t svc_handle = 1;
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{svc_handle});
  svc_info.set_type(fbt::Uuid{{5}});
  fbg::Characteristic chrc;
  fbg::Handle chrc_handle{2};
  chrc.set_handle(chrc_handle);
  chrc.set_type(fbt::Uuid{{0}});
  chrc.set_properties(fbg::CharacteristicPropertyBits::WRITE);
  chrc.set_permissions(fbg::AttributePermissions());
  std::vector<fbg::Characteristic> chrcs;
  chrcs.push_back(std::move(chrc));
  svc_info.set_characteristics(std::move(chrcs));

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc(local_service_handle.NewRequest());

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();
  ASSERT_EQ(svc_iter->second.service->characteristics().size(), 1u);

  bt::gatt::IdType svc_id = svc_iter->first;
  bt::gatt::IdType chrc_id = svc_iter->second.service->characteristics()[0]->id();
  bt::PeerId peer_id(99);
  bt::StaticByteBuffer value_buffer(0x00, 0x01, 0x02);

  int write_value_count = 0;
  local_svc.set_write_value_function(
      [&](fbg::LocalServiceWriteValueRequest req, fbg::LocalService::WriteValueCallback cb) {
        write_value_count++;
        ASSERT_TRUE(req.has_peer_id());
        EXPECT_EQ(req.peer_id().value, peer_id.value());
        ASSERT_TRUE(req.has_handle());
        EXPECT_EQ(req.handle().value, chrc_handle.value);
        ASSERT_TRUE(req.has_offset());
        EXPECT_EQ(req.offset(), 3u);
        ASSERT_TRUE(req.has_value());
        EXPECT_THAT(req.value(), ::testing::ElementsAreArray(value_buffer));
        cb(fpromise::ok());
      });
  int write_responder_count = 0;
  auto write_responder = [&](fit::result<bt::att::ErrorCode> status) {
    write_responder_count++;
    EXPECT_TRUE(status.is_ok());
  };
  svc_iter->second.write_handler(peer_id, svc_id, chrc_id, /*offset=*/3, value_buffer,
                                 std::move(write_responder));
  RunLoopUntilIdle();
  EXPECT_EQ(write_value_count, 1);
  EXPECT_EQ(write_responder_count, 1);
}

TEST_F(Gatt2ServerServerTest, WriteError) {
  uint64_t svc_handle = 1;
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{svc_handle});
  svc_info.set_type(fbt::Uuid{{5}});
  fbg::Characteristic chrc;
  fbg::Handle chrc_handle{2};
  chrc.set_handle(chrc_handle);
  chrc.set_type(fbt::Uuid{{0}});
  chrc.set_properties(fbg::CharacteristicPropertyBits::WRITE);
  chrc.set_permissions(fbg::AttributePermissions());
  std::vector<fbg::Characteristic> chrcs;
  chrcs.push_back(std::move(chrc));
  svc_info.set_characteristics(std::move(chrcs));

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc(local_service_handle.NewRequest());

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();
  ASSERT_EQ(svc_iter->second.service->characteristics().size(), 1u);

  bt::gatt::IdType svc_id = svc_iter->first;
  bt::gatt::IdType chrc_id = svc_iter->second.service->characteristics()[0]->id();

  int write_value_count = 0;
  local_svc.set_write_value_function(
      [&](fbg::LocalServiceWriteValueRequest req, fbg::LocalService::WriteValueCallback cb) {
        write_value_count++;
        cb(fpromise::error(fbg::Error::WRITE_NOT_PERMITTED));
      });
  int write_responder_count = 0;
  auto write_responder = [&](fit::result<bt::att::ErrorCode> status) {
    write_responder_count++;
    ASSERT_TRUE(status.is_error());
    EXPECT_EQ(status.error_value(), bt::att::ErrorCode::kWriteNotPermitted);
  };
  svc_iter->second.write_handler(bt::PeerId(4), svc_id, chrc_id, /*offset=*/0, bt::BufferView(),
                                 std::move(write_responder));
  RunLoopUntilIdle();
  EXPECT_EQ(write_value_count, 1);
  EXPECT_EQ(write_responder_count, 1);
}

TEST_F(Gatt2ServerServerTest, ClientCharacteristicConfiguration) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();
  bt::gatt::IdType svc_id = svc_iter->first;

  bt::PeerId peer_id(4);
  int ccc_count = 0;
  local_svc.set_ccc_function([&](fbt::PeerId cb_peer_id, fbg::Handle handle, bool notify,
                                 bool indicate,
                                 fbg::LocalService::CharacteristicConfigurationCallback cb) {
    ccc_count++;
    EXPECT_EQ(peer_id.value(), cb_peer_id.value);
    EXPECT_EQ(handle.value, chrc_handle);
    EXPECT_TRUE(notify);
    EXPECT_FALSE(indicate);
    cb();
  });
  svc_iter->second.ccc_callback(svc_id, chrc_handle, peer_id, /*notify=*/true, /*indicate=*/false);
  RunLoopUntilIdle();
  EXPECT_EQ(ccc_count, 1);
}

TEST_F(Gatt2ServerServerTest, IndicateAllPeers) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  fbg::ValueChangedParameters params_0;
  params_0.set_handle(fbg::Handle{chrc_handle});
  std::vector<uint8_t> buffer = {0x00, 0x01, 0x02};
  params_0.set_value(buffer);
  zx::eventpair confirm_ours_0;
  zx::eventpair confirm_theirs_0;
  zx::eventpair::create(/*options=*/0, &confirm_ours_0, &confirm_theirs_0);
  local_svc.IndicateValue(std::move(params_0), std::move(confirm_theirs_0));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 1u);
  EXPECT_EQ(svc_iter->second.updates[0].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[0].value, buffer);

  // Eventpair should not be signalled until indicate_cb called.
  zx_signals_t observed;
  zx_status_t status = confirm_ours_0.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                               /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);
  EXPECT_EQ(observed, 0u);

  svc_iter->second.updates[0].indicate_cb(fit::ok());

  status = confirm_ours_0.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                   /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  // The eventpair should have been signaled before it was closed.
  EXPECT_EQ(observed, ZX_EVENTPAIR_SIGNALED | ZX_EVENTPAIR_PEER_CLOSED);
  observed = 0;

  // Also test with an empty peer_ids vector
  fbg::ValueChangedParameters params_1;
  params_1.set_peer_ids({});
  params_1.set_handle(fbg::Handle{chrc_handle});
  params_1.set_value(buffer);
  zx::eventpair confirm_ours_1;
  zx::eventpair confirm_theirs_1;
  zx::eventpair::create(/*options=*/0, &confirm_ours_1, &confirm_theirs_1);
  local_svc.IndicateValue(std::move(params_1), std::move(confirm_theirs_1));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 2u);
  status = confirm_ours_1.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                   /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);
  EXPECT_EQ(observed, 0u);

  svc_iter->second.updates[1].indicate_cb(fit::ok());

  status = confirm_ours_1.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                   /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_SIGNALED | ZX_EVENTPAIR_PEER_CLOSED);
}

TEST_F(Gatt2ServerServerTest, IndicateAllPeersError) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  fbg::ValueChangedParameters params;
  params.set_handle(fbg::Handle{chrc_handle});
  std::vector<uint8_t> buffer = {0x00, 0x01, 0x02};
  params.set_value(buffer);
  zx::eventpair confirm_ours;
  zx::eventpair confirm_theirs;
  zx::eventpair::create(/*options=*/0, &confirm_ours, &confirm_theirs);
  local_svc.IndicateValue(std::move(params), std::move(confirm_theirs));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 1u);
  EXPECT_EQ(svc_iter->second.updates[0].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[0].value, buffer);

  // Eventpair should not be signalled until indicate_cb called.
  zx_signals_t observed;
  zx_status_t status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                             /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);
  EXPECT_EQ(observed, 0u);

  svc_iter->second.updates[0].indicate_cb(fit::error(bt::att::ErrorCode::kUnlikelyError));

  status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                 /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_PEER_CLOSED);
}

TEST_F(Gatt2ServerServerTest, IndicateValueChangedParametersMissingHandleClosesService) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  ASSERT_FALSE(local_svc.error());

  // No handle is set.
  fbg::ValueChangedParameters params_0;
  std::vector<uint8_t> buffer = {0x00, 0x01, 0x02};
  params_0.set_value(buffer);
  zx::eventpair confirm_ours_0;
  zx::eventpair confirm_theirs_0;
  zx::eventpair::create(/*options=*/0, &confirm_ours_0, &confirm_theirs_0);
  local_svc.IndicateValue(std::move(params_0), std::move(confirm_theirs_0));
  RunLoopUntilIdle();
  ASSERT_TRUE(local_svc.error());
  EXPECT_EQ(local_svc.error().value(), ZX_ERR_PEER_CLOSED);
  EXPECT_TRUE(gatt()->local_services().empty());
  zx_signals_t observed;
  zx_status_t status = confirm_ours_0.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                               /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_PEER_CLOSED);
}

TEST_F(Gatt2ServerServerTest, IndicateValueChangedParametersMissingValueClosesService) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_FALSE(local_svc.error());
  ASSERT_EQ(gatt()->local_services().size(), 1u);

  // No value is set.
  fbg::ValueChangedParameters params_1;
  params_1.set_handle(fbg::Handle{chrc_handle});
  zx::eventpair confirm_ours_1;
  zx::eventpair confirm_theirs_1;
  zx::eventpair::create(/*options=*/0, &confirm_ours_1, &confirm_theirs_1);
  local_svc.IndicateValue(std::move(params_1), std::move(confirm_theirs_1));
  RunLoopUntilIdle();
  ASSERT_TRUE(local_svc.error());
  EXPECT_EQ(local_svc.error().value(), ZX_ERR_PEER_CLOSED);
  EXPECT_TRUE(gatt()->local_services().empty());
  zx_signals_t observed;
  zx_status_t status = confirm_ours_1.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                               /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_PEER_CLOSED);
}

TEST_F(Gatt2ServerServerTest, Indicate2PeersSuccess) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  bt::PeerId peer_0(0);
  bt::PeerId peer_1(1);
  fbg::ValueChangedParameters params;
  params.set_handle(fbg::Handle{chrc_handle});
  std::vector<uint8_t> buffer = {0x00, 0x01, 0x02};
  params.set_value(buffer);
  params.set_peer_ids({fbt::PeerId{peer_0.value()}, fbt::PeerId{peer_1.value()}});
  zx::eventpair confirm_ours;
  zx::eventpair confirm_theirs;
  zx::eventpair::create(/*options=*/0, &confirm_ours, &confirm_theirs);
  local_svc.IndicateValue(std::move(params), std::move(confirm_theirs));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 2u);
  EXPECT_EQ(svc_iter->second.updates[0].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[0].value, buffer);
  EXPECT_THAT(svc_iter->second.updates[0].peer, ::testing::Optional(peer_0));
  EXPECT_EQ(svc_iter->second.updates[1].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[1].value, buffer);
  EXPECT_THAT(svc_iter->second.updates[1].peer, ::testing::Optional(peer_1));

  // Eventpair should not be signaled until indicate_cb called for both peers.
  zx_signals_t observed;
  zx_status_t status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                             /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);

  svc_iter->second.updates[1].indicate_cb(fit::ok());

  status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                 /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);

  svc_iter->second.updates[0].indicate_cb(fit::ok());

  status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                 /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED);
}

TEST_F(Gatt2ServerServerTest, Indicate2PeersFirstOneFails) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  fbg::ValueChangedParameters params;
  params.set_handle(fbg::Handle{chrc_handle});
  params.set_value(kBuffer);
  params.set_peer_ids({fbt::PeerId{0}, fbt::PeerId{1}});
  zx::eventpair confirm_ours;
  zx::eventpair confirm_theirs;
  zx::eventpair::create(/*options=*/0, &confirm_ours, &confirm_theirs);
  local_svc.IndicateValue(std::move(params), std::move(confirm_theirs));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 2u);

  zx_signals_t observed;
  zx_status_t status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                             /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);

  svc_iter->second.updates[0].indicate_cb(fit::error(bt::att::ErrorCode::kUnlikelyError));

  status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                 /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_PEER_CLOSED);

  svc_iter->second.updates[1].indicate_cb(fit::ok());
}

TEST_F(Gatt2ServerServerTest, Indicate2PeersBothFail) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  fbg::ValueChangedParameters params;
  params.set_handle(fbg::Handle{chrc_handle});
  params.set_value(kBuffer);
  params.set_peer_ids({fbt::PeerId{0}, fbt::PeerId{1}});
  zx::eventpair confirm_ours;
  zx::eventpair confirm_theirs;
  zx::eventpair::create(/*options=*/0, &confirm_ours, &confirm_theirs);
  local_svc.IndicateValue(std::move(params), std::move(confirm_theirs));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 2u);

  zx_signals_t observed;
  zx_status_t status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                             /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_ERR_TIMED_OUT);

  svc_iter->second.updates[1].indicate_cb(fit::error(bt::att::ErrorCode::kUnlikelyError));

  status = confirm_ours.wait_one(ZX_EVENTPAIR_PEER_CLOSED | ZX_EVENTPAIR_SIGNALED,
                                 /*deadline=*/zx::time(0), &observed);
  ASSERT_EQ(status, ZX_OK);
  EXPECT_EQ(observed, ZX_EVENTPAIR_PEER_CLOSED);

  svc_iter->second.updates[0].indicate_cb(fit::error(bt::att::ErrorCode::kUnlikelyError));
}

TEST_F(Gatt2ServerServerTest, NotifyAllPeers) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  fbg::ValueChangedParameters params_0;
  params_0.set_handle(fbg::Handle{chrc_handle});
  params_0.set_value(kBuffer);
  local_svc.NotifyValue(std::move(params_0));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 1u);
  EXPECT_EQ(svc_iter->second.updates[0].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[0].value, kBuffer);
  EXPECT_FALSE(svc_iter->second.updates[0].peer);
  EXPECT_FALSE(svc_iter->second.updates[0].indicate_cb);

  // Also test with an empty peer_ids vector
  fbg::ValueChangedParameters params_1;
  params_1.set_peer_ids({});
  params_1.set_handle(fbg::Handle{chrc_handle});
  params_1.set_value(kBuffer);
  local_svc.NotifyValue(std::move(params_1));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 2u);
  EXPECT_EQ(svc_iter->second.updates[1].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[1].value, kBuffer);
  EXPECT_FALSE(svc_iter->second.updates[1].peer);
  EXPECT_FALSE(svc_iter->second.updates[1].indicate_cb);
}

TEST_F(Gatt2ServerServerTest, NotifyTwoPeers) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  fbg::ValueChangedParameters params_0;
  params_0.set_handle(fbg::Handle{chrc_handle});
  params_0.set_value(kBuffer);
  params_0.set_peer_ids({fbt::PeerId{0}, fbt::PeerId{1}});
  local_svc.NotifyValue(std::move(params_0));
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 2u);
  EXPECT_EQ(svc_iter->second.updates[0].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[0].value, kBuffer);
  EXPECT_THAT(svc_iter->second.updates[0].peer, ::testing::Optional(bt::PeerId(0)));
  EXPECT_FALSE(svc_iter->second.updates[0].indicate_cb);
  EXPECT_EQ(svc_iter->second.updates[1].chrc_id, chrc_handle);
  EXPECT_EQ(svc_iter->second.updates[1].value, kBuffer);
  EXPECT_THAT(svc_iter->second.updates[1].peer, ::testing::Optional(bt::PeerId(1)));
  EXPECT_FALSE(svc_iter->second.updates[1].indicate_cb);
}

TEST_F(Gatt2ServerServerTest, NotifyInvalidParametersMissingHandleClosesService) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  ASSERT_FALSE(local_svc.error());

  // Missing handle.
  fbg::ValueChangedParameters params_0;
  params_0.set_value(kBuffer);
  local_svc.NotifyValue(std::move(params_0));
  RunLoopUntilIdle();
  ASSERT_TRUE(local_svc.error());
  EXPECT_EQ(local_svc.error().value(), ZX_ERR_PEER_CLOSED);
  EXPECT_TRUE(gatt()->local_services().empty());
}

TEST_F(Gatt2ServerServerTest, NotifyInvalidParametersMissingValueClosesService) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  ASSERT_FALSE(local_svc.error());

  // Missing value.
  fbg::ValueChangedParameters params_1;
  params_1.set_handle(fbg::Handle{chrc_handle});
  local_svc.NotifyValue(std::move(params_1));
  RunLoopUntilIdle();
  ASSERT_TRUE(local_svc.error());
  EXPECT_EQ(local_svc.error().value(), ZX_ERR_PEER_CLOSED);
  EXPECT_TRUE(gatt()->local_services().empty());
}

TEST_F(Gatt2ServerServerTest, ValueChangedFlowControl) {
  const uint64_t chrc_handle = 2;
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, /*svc_type=*/{5}, chrc_handle);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  MockLocalService local_svc = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    EXPECT_TRUE(res.is_response());
    cb_count++;
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 1u);
  auto svc_iter = gatt()->local_services().begin();

  for (size_t i = 0; i < 3 * fbg::INITIAL_VALUE_CHANGED_CREDITS; i++) {
    fbg::ValueChangedParameters params;
    params.set_handle(fbg::Handle{chrc_handle});
    params.set_value(kBuffer);
    local_svc.NotifyValue(std::move(params));
  }
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 3 * fbg::INITIAL_VALUE_CHANGED_CREDITS);
  EXPECT_GT(local_svc.credits(), 0);
  EXPECT_LE(local_svc.credits(), static_cast<int>(fbg::INITIAL_VALUE_CHANGED_CREDITS));
  EXPECT_GE(local_svc.credit_log().size(), 2u);

  for (size_t i = 0; i < 3 * fbg::INITIAL_VALUE_CHANGED_CREDITS; i++) {
    fbg::ValueChangedParameters params;
    params.set_handle(fbg::Handle{chrc_handle});
    params.set_value(kBuffer);
    zx::eventpair confirm_ours;
    zx::eventpair confirm_theirs;
    zx::eventpair::create(/*options=*/0, &confirm_ours, &confirm_theirs);
    local_svc.IndicateValue(std::move(params), std::move(confirm_theirs));
  }
  RunLoopUntilIdle();
  ASSERT_EQ(svc_iter->second.updates.size(), 6 * fbg::INITIAL_VALUE_CHANGED_CREDITS);
  EXPECT_GT(local_svc.credits(), 0);
  EXPECT_LE(local_svc.credits(), static_cast<int>(fbg::INITIAL_VALUE_CHANGED_CREDITS));
  EXPECT_GE(local_svc.credit_log().size(), 5u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceWithInvalidCharacteristic) {
  fbg::ServiceInfo svc_info;
  svc_info.set_handle(fbg::ServiceHandle{1});
  bt::UInt128 svc_type = {5};
  svc_info.set_type(fbt::Uuid{svc_type});

  fbg::Characteristic chrc_0;
  fbg::Handle chrc_handle_0{2};
  chrc_0.set_handle(chrc_handle_0);

  std::vector<fbg::Characteristic> characteristics;
  characteristics.push_back(std::move(chrc_0));
  svc_info.set_characteristics(std::move(characteristics));

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle_0;
  fidl::InterfaceRequest<fbg::LocalService> request = local_service_handle_0.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    cb_count++;
    ASSERT_TRUE(res.is_err());
    EXPECT_EQ(res.err(), fbg::PublishServiceError::INVALID_CHARACTERISTICS);
  };
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle_0),
                               std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 0u);
}

TEST_F(Gatt2ServerServerTest, PublishServiceReturnsInvalidId) {
  bt::UInt128 svc_type = {5};
  fbg::ServiceInfo svc_info = BuildSimpleService(/*svc_handle=*/1, svc_type);

  fidl::InterfaceHandle<fbg::LocalService> local_service_handle;
  fidl::InterfaceRequest<fbg::LocalService> request = local_service_handle.NewRequest();

  int cb_count = 0;
  auto cb = [&](fbg::Server_PublishService_Result res) {
    cb_count++;
    ASSERT_TRUE(res.is_err());
    EXPECT_EQ(res.err(), fbg::PublishServiceError::UNLIKELY_ERROR);
  };

  gatt()->set_register_service_fails(true);
  server_ptr()->PublishService(std::move(svc_info), std::move(local_service_handle), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1);
  ASSERT_EQ(gatt()->local_services().size(), 0u);
}

}  // namespace
}  // namespace bthost
