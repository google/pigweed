// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_server_server.h"

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/server.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;
using GattErrorCode = fuchsia::bluetooth::gatt::ErrorCode;

using fuchsia::bluetooth::gatt::Characteristic;
using fuchsia::bluetooth::gatt::Descriptor;
using fuchsia::bluetooth::gatt::LocalService;
using fuchsia::bluetooth::gatt::LocalServiceDelegate;
using fuchsia::bluetooth::gatt::LocalServiceDelegatePtr;
using fuchsia::bluetooth::gatt::SecurityRequirementsPtr;
using fuchsia::bluetooth::gatt::ServiceInfo;

namespace bthost {
namespace {

fitx::result<bt::att::ErrorCode> GattStatusFromFidl(GattErrorCode error_code, bool is_read) {
  switch (error_code) {
    case GattErrorCode::NO_ERROR:
      return fitx::ok();
    case GattErrorCode::INVALID_OFFSET:
      return fitx::error(bt::att::ErrorCode::kInvalidOffset);
    case GattErrorCode::INVALID_VALUE_LENGTH:
      return fitx::error(bt::att::ErrorCode::kInvalidAttributeValueLength);
    case GattErrorCode::NOT_PERMITTED:
      if (is_read)
        return fitx::error(bt::att::ErrorCode::kReadNotPermitted);
      return fitx::error(bt::att::ErrorCode::kWriteNotPermitted);
    default:
      break;
  }
  return fitx::error(bt::att::ErrorCode::kUnlikelyError);
}

bt::att::AccessRequirements ParseSecurityRequirements(const SecurityRequirementsPtr& reqs) {
  if (!reqs) {
    return bt::att::AccessRequirements();
  }
  return bt::att::AccessRequirements(reqs->encryption_required, reqs->authentication_required,
                                     reqs->authorization_required);
}

// Carries a either a successful Result or an error message that can be sent as
// a FIDL response.
template <typename Result, typename Error = std::string,
          typename = std::enable_if_t<!std::is_same<Result, Error>::value>>
struct MaybeResult final {
  explicit MaybeResult(Result&& result) : result(std::forward<Result>(result)) {}

  explicit MaybeResult(Error&& error) : error(std::forward<Error>(error)) {}

  bool is_error() const { return static_cast<bool>(!result); }

  Result result;
  Error error;
};

using DescriptorResult = MaybeResult<bt::gatt::DescriptorPtr>;
DescriptorResult NewDescriptor(const Descriptor& fidl_desc) {
  auto read_reqs = ParseSecurityRequirements(fidl_desc.permissions->read);
  auto write_reqs = ParseSecurityRequirements(fidl_desc.permissions->write);

  bt::UUID type;
  if (!bt::StringToUuid(fidl_desc.type, &type)) {
    return DescriptorResult("Invalid descriptor UUID");
  }

  return DescriptorResult(
      std::make_unique<bt::gatt::Descriptor>(fidl_desc.id, type, read_reqs, write_reqs));
}

using CharacteristicResult = MaybeResult<bt::gatt::CharacteristicPtr>;
CharacteristicResult NewCharacteristic(const Characteristic& fidl_chrc) {
  uint8_t props = fidl_chrc.properties & 0xFF;
  uint16_t ext_props = (fidl_chrc.properties & 0xFF00) >> 8;

  if (!fidl_chrc.permissions) {
    return CharacteristicResult("Characteristic permissions missing");
  }

  bool supports_update =
      (props & bt::gatt::Property::kNotify) || (props & bt::gatt::Property::kIndicate);
  if (supports_update != static_cast<bool>(fidl_chrc.permissions->update)) {
    return CharacteristicResult(supports_update ? "Characteristic update permission required"
                                                : "Characteristic update permission must be null");
  }

  auto read_reqs = ParseSecurityRequirements(fidl_chrc.permissions->read);
  auto write_reqs = ParseSecurityRequirements(fidl_chrc.permissions->write);
  auto update_reqs = ParseSecurityRequirements(fidl_chrc.permissions->update);

  bt::UUID type;
  if (!bt::StringToUuid(fidl_chrc.type, &type)) {
    return CharacteristicResult("Invalid characteristic UUID");
  }

  auto chrc = std::make_unique<bt::gatt::Characteristic>(fidl_chrc.id, type, props, ext_props,
                                                         read_reqs, write_reqs, update_reqs);
  if (fidl_chrc.descriptors && !fidl_chrc.descriptors->empty()) {
    for (const auto& fidl_desc : *fidl_chrc.descriptors) {
      auto desc_result = NewDescriptor(fidl_desc);
      if (desc_result.is_error()) {
        return CharacteristicResult(std::move(desc_result.error));
      }

      chrc->AddDescriptor(std::move(desc_result.result));
    }
  }

  return CharacteristicResult(std::move(chrc));
}

}  // namespace

// Implements the gatt::LocalService FIDL interface. Instances of this class are
// only created by a GattServerServer.
class GattServerServer::LocalServiceImpl
    : public GattServerBase<fuchsia::bluetooth::gatt::LocalService> {
 public:
  LocalServiceImpl(GattServerServer* owner, uint64_t id, LocalServiceDelegatePtr delegate,
                   ::fidl::InterfaceRequest<LocalService> request)
      : GattServerBase(owner->gatt(), this, std::move(request)),
        owner_(owner),
        id_(id),
        delegate_(std::move(delegate)) {
    BT_DEBUG_ASSERT(owner_);
    BT_DEBUG_ASSERT(delegate_);
  }

  // The destructor removes the GATT service
  ~LocalServiceImpl() override {
    CleanUp();

    // Do not notify the owner in this case. If we got here it means that
    // |owner_| deleted us.
  }

  // Returns the current delegate. Returns nullptr if the delegate was
  // disconnected (e.g. due to a call to RemoveService()).
  LocalServiceDelegate* delegate() { return delegate_.get(); }

 private:
  // fuchsia::bluetooth::gatt::Service overrides:
  void RemoveService() override {
    CleanUp();
    owner_->RemoveService(id_);
  }

  void NotifyValue(uint64_t characteristic_id, ::std::string peer_id, ::std::vector<uint8_t> value,
                   bool confirm) override {
    auto id = fidl_helpers::PeerIdFromString(std::move(peer_id));
    if (id) {
      bt::gatt::IndicationCallback indication_cb = nullptr;
      if (confirm) {
        indication_cb = [](bt::att::Result<> result) {
          bt_log(DEBUG, "fidl", "indication result: %s", bt_str(result));
        };
      }
      gatt()->SendUpdate(id_, characteristic_id, *id, std::move(value), std::move(indication_cb));
    }
  }

  // Unregisters the underlying service if it is still active.
  void CleanUp() {
    delegate_ = nullptr;  // Closes the delegate handle.
    gatt()->UnregisterService(id_);
  }

  // |owner_| owns this instance and is expected to outlive it.
  GattServerServer* owner_;  // weak
  uint64_t id_;

  // The delegate connection for the corresponding service instance. This gets
  // cleared when the service is unregistered (via RemoveService() or
  // destruction).
  LocalServiceDelegatePtr delegate_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LocalServiceImpl);
};

GattServerServer::GattServerServer(fxl::WeakPtr<bt::gatt::GATT> gatt,
                                   fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> request)
    : GattServerBase(gatt, this, std::move(request)), weak_ptr_factory_(this) {}

GattServerServer::~GattServerServer() {
  // This will remove all of our services from their adapter.
  services_.clear();
}

void GattServerServer::RemoveService(uint64_t id) {
  if (services_.erase(id)) {
    bt_log(DEBUG, "fidl", "%s: service removed (id: %lu)", __FUNCTION__, id);
  } else {
    bt_log(WARN, "fidl", "%s: service id not found: %lu", __FUNCTION__, id);
  }
}

void GattServerServer::PublishService(ServiceInfo service_info,
                                      fidl::InterfaceHandle<LocalServiceDelegate> delegate,
                                      fidl::InterfaceRequest<LocalService> service_iface,
                                      PublishServiceCallback callback) {
  if (!delegate) {
    bt_log(WARN, "fidl", "%s: missing service delegate", __FUNCTION__);
    auto error = fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "A delegate is required");
    callback(std::move(error));
    return;
  }

  if (!service_iface) {
    bt_log(WARN, "fidl", "%s: missing service interface request", __FUNCTION__);
    auto error =
        fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "Service interface is required");
    callback(std::move(error));
    return;
  }

  bt::UUID service_type;
  if (!bt::StringToUuid(service_info.type, &service_type)) {
    bt_log(WARN, "fidl", "%s: invalid service UUID %s", __FUNCTION__, service_info.type.c_str());
    auto error = fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "Invalid service UUID");
    callback(std::move(error));
    return;
  }

  // Process the FIDL service tree.
  auto service = std::make_unique<bt::gatt::Service>(service_info.primary, service_type);
  if (service_info.characteristics) {
    for (const auto& fidl_chrc : *service_info.characteristics) {
      auto chrc_result = NewCharacteristic(fidl_chrc);
      if (chrc_result.is_error()) {
        auto error = fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, chrc_result.error);
        callback(std::move(error));
        return;
      }

      service->AddCharacteristic(std::move(chrc_result.result));
    }
  }

  auto self = weak_ptr_factory_.GetWeakPtr();

  // Set up event handlers.
  auto read_handler = [self](bt::PeerId /*ignore*/, auto svc_id, auto id, auto offset,
                             auto responder) mutable {
    if (self) {
      self->OnReadRequest(svc_id, id, offset, std::move(responder));
    } else {
      responder(fitx::error(bt::att::ErrorCode::kUnlikelyError), bt::BufferView());
    }
  };
  auto write_handler = [self](bt::PeerId /*ignore*/, auto svc_id, auto id, auto offset,
                              const auto& value, auto responder) mutable {
    if (self) {
      self->OnWriteRequest(svc_id, id, offset, value, std::move(responder));
    } else {
      responder(fitx::error(bt::att::ErrorCode::kUnlikelyError));
    }
  };
  auto ccc_callback = [self](auto svc_id, auto id, bt::gatt::PeerId peer_id, bool notify,
                             bool indicate) {
    if (self)
      self->OnCharacteristicConfig(svc_id, id, peer_id, notify, indicate);
  };

  auto id_cb = [self, delegate = std::move(delegate), service_iface = std::move(service_iface),
                callback = std::move(callback)](bt::gatt::IdType id) mutable {
    if (!self)
      return;

    if (!id) {
      // TODO(armansito): Report a more detailed string if registration
      // fails due to duplicate ids.
      auto error = fidl_helpers::NewFidlError(ErrorCode::FAILED, "Failed to publish service");
      callback(std::move(error));
      return;
    }

    BT_DEBUG_ASSERT(self->services_.find(id) == self->services_.end());

    // This will be called if either the delegate or the service connection
    // closes.
    auto connection_error_cb = [self, id](zx_status_t status) {
      bt_log(DEBUG, "bt-host", "removing GATT service (id: %lu)", id);
      if (self)
        self->RemoveService(id);
    };

    auto delegate_ptr = delegate.Bind();
    delegate_ptr.set_error_handler(connection_error_cb);

    auto service_server = std::make_unique<LocalServiceImpl>(
        self.get(), id, std::move(delegate_ptr), std::move(service_iface));
    service_server->set_error_handler(connection_error_cb);
    self->services_[id] = std::move(service_server);

    callback(Status());
  };

  gatt()->RegisterService(std::move(service), std::move(id_cb), std::move(read_handler),
                          std::move(write_handler), std::move(ccc_callback));
}

void GattServerServer::OnReadRequest(bt::gatt::IdType service_id, bt::gatt::IdType id,
                                     uint16_t offset, bt::gatt::ReadResponder responder) {
  auto iter = services_.find(service_id);
  if (iter == services_.end()) {
    bt_log(WARN, "fidl", "%s: unknown service id %lu", __FUNCTION__, service_id);
    responder(fitx::error(bt::att::ErrorCode::kUnlikelyError), bt::BufferView());
    return;
  }

  auto cb = [responder = std::move(responder)](fidl::VectorPtr<uint8_t> optional_value,
                                               auto error_code) mutable {
    std::vector<uint8_t> value;
    if (optional_value.has_value()) {
      value = std::move(optional_value.value());
    }
    responder(GattStatusFromFidl(error_code, /*is_read=*/true),
              bt::BufferView(value.data(), value.size()));
  };

  auto* delegate = iter->second->delegate();
  BT_DEBUG_ASSERT(delegate);
  delegate->OnReadValue(id, offset, std::move(cb));
}

void GattServerServer::OnWriteRequest(bt::gatt::IdType service_id, bt::gatt::IdType id,
                                      uint16_t offset, const bt::ByteBuffer& value,
                                      bt::gatt::WriteResponder responder) {
  auto iter = services_.find(service_id);
  if (iter == services_.end()) {
    bt_log(WARN, "fidl", "%s: unknown service id %lu", __FUNCTION__, service_id);
    responder(fitx::error(bt::att::ErrorCode::kUnlikelyError));
    return;
  }

  auto fidl_value = fidl::To<std::vector<uint8_t>>(value);
  auto* delegate = iter->second->delegate();
  BT_DEBUG_ASSERT(delegate);

  if (!responder) {
    delegate->OnWriteWithoutResponse(id, offset, std::move(fidl_value));
    return;
  }

  auto cb = [responder = std::move(responder)](auto error_code) mutable {
    responder(GattStatusFromFidl(error_code, /*is_read=*/false));
  };

  delegate->OnWriteValue(id, offset, std::move(fidl_value), std::move(cb));
}

void GattServerServer::OnCharacteristicConfig(bt::gatt::IdType service_id, bt::gatt::IdType chrc_id,
                                              bt::gatt::PeerId peer_id, bool notify,
                                              bool indicate) {
  auto iter = services_.find(service_id);
  if (iter == services_.end()) {
    bt_log(WARN, "fidl", "%s: unknown service id %lu", __FUNCTION__, service_id);
    return;
  }

  auto* delegate = iter->second->delegate();
  BT_DEBUG_ASSERT(delegate);
  delegate->OnCharacteristicConfiguration(chrc_id, peer_id.ToString(), notify, indicate);
}

}  // namespace bthost
