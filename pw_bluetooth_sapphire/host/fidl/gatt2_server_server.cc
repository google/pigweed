// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt2_server_server.h"

#include <functional>
#include <utility>

#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/gatt2/cpp/fidl.h"
#include "lib/fidl/cpp/interface_ptr.h"
#include "lib/fit/defer.h"
#include "lib/fit/function.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/gatt2_server_ids.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/server.h"

namespace fbt = fuchsia::bluetooth;
namespace btg = bt::gatt;

using fuchsia::bluetooth::gatt2::AttributePermissions;
using fuchsia::bluetooth::gatt2::Characteristic;
using fuchsia::bluetooth::gatt2::Descriptor;
using fgatt2Err = fuchsia::bluetooth::gatt2::Error;
using fuchsia::bluetooth::gatt2::Handle;
using fuchsia::bluetooth::gatt2::INITIAL_VALUE_CHANGED_CREDITS;
using fuchsia::bluetooth::gatt2::LocalService;
using fuchsia::bluetooth::gatt2::LocalService_ReadValue_Result;
using fuchsia::bluetooth::gatt2::LocalService_WriteValue_Result;
using fuchsia::bluetooth::gatt2::LocalServicePeerUpdateRequest;
using fuchsia::bluetooth::gatt2::LocalServiceWriteValueRequest;
using fuchsia::bluetooth::gatt2::PublishServiceError;
using fuchsia::bluetooth::gatt2::SecurityRequirements;
using fuchsia::bluetooth::gatt2::ServiceInfo;
using fuchsia::bluetooth::gatt2::ServiceKind;
using fuchsia::bluetooth::gatt2::ValueChangedParameters;

namespace bthost {

Gatt2ServerServer::Gatt2ServerServer(
    fxl::WeakPtr<btg::GATT> gatt, fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::Server> request)
    : GattServerBase(std::move(gatt), this, std::move(request)), weak_ptr_factory_(this) {}

Gatt2ServerServer::~Gatt2ServerServer() {
  // Remove all services from the local GATT host.
  for (const auto& iter : services_) {
    gatt()->UnregisterService(iter.first.value());
  }
}

void Gatt2ServerServer::RemoveService(InternalServiceId id) {
  // Close the connection to the FIDL LocalService.
  services_.erase(id);
  // Unregister the service from the local GATT host. Don't remove the ID from service_id_mapping_
  // to prevent service ID reuse.
  gatt()->UnregisterService(id.value());
}

void Gatt2ServerServer::PublishService(ServiceInfo info,
                                       fidl::InterfaceHandle<LocalService> service,
                                       PublishServiceCallback callback) {
  if (!info.has_handle()) {
    bt_log(WARN, "fidl", "%s: `info` missing required `handle`", __FUNCTION__);
    callback(fpromise::error(PublishServiceError::INVALID_SERVICE_HANDLE));
    return;
  }
  if (!info.has_type()) {
    bt_log(WARN, "fidl", "%s: `info` missing required `type` UUID", __FUNCTION__);
    callback(fpromise::error(PublishServiceError::INVALID_UUID));
    return;
  }
  if (!info.has_characteristics()) {
    bt_log(WARN, "fidl", "%s: `info` missing required `characteristics`", __FUNCTION__);
    callback(fpromise::error(PublishServiceError::INVALID_CHARACTERISTICS));
    return;
  }

  ClientServiceId client_svc_id(info.handle().value);
  if (service_id_mapping_.count(client_svc_id) != 0) {
    bt_log(WARN, "fidl", "%s: cannot publish service with already-used `handle`", __FUNCTION__);
    callback(fpromise::error(PublishServiceError::INVALID_SERVICE_HANDLE));
    return;
  }

  bt::UUID service_type(info.type().value);
  // The default value for kind is PRIMARY if not present.
  bool primary = info.has_kind() ? info.kind() == ServiceKind::PRIMARY : true;

  // Process the FIDL service tree.
  auto gatt_svc = std::make_unique<btg::Service>(primary, service_type);
  for (const auto& fidl_chrc : info.characteristics()) {
    btg::CharacteristicPtr maybe_chrc = fidl_helpers::Gatt2CharacteristicFromFidl(fidl_chrc);
    if (!maybe_chrc) {
      callback(fpromise::error(PublishServiceError::INVALID_CHARACTERISTICS));
      return;
    }

    gatt_svc->AddCharacteristic(std::move(maybe_chrc));
  }

  fxl::WeakPtr<Gatt2ServerServer> self = weak_ptr_factory_.GetWeakPtr();
  auto id_cb = [self, service_handle = std::move(service), client_svc_id,
                callback = std::move(callback)](btg::IdType raw_id) mutable {
    if (!self) {
      return;
    }

    if (raw_id == bt::gatt::kInvalidId) {
      bt_log(INFO, "bt-host", "internal error publishing service (handle: %lu)",
             client_svc_id.value());
      callback(fpromise::error(PublishServiceError::UNLIKELY_ERROR));
      return;
    }
    InternalServiceId internal_id(raw_id);
    auto error_handler = [self, client_svc_id, internal_id](zx_status_t status) {
      bt_log(INFO, "bt-host", "LocalService shut down, removing GATT service (id: %lu)",
             client_svc_id.value());
      self->RemoveService(internal_id);
    };

    fidl::InterfacePtr<LocalService> service_ptr = service_handle.Bind();
    service_ptr.set_error_handler(error_handler);
    service_ptr.events().OnSuppressDiscovery = [self, internal_id]() {
      self->OnSuppressDiscovery(internal_id);
    };
    service_ptr.events().OnNotifyValue = [self, internal_id](ValueChangedParameters update) {
      self->OnNotifyValue(internal_id, std::move(update));
    };
    service_ptr.events().OnIndicateValue = [self, internal_id](ValueChangedParameters update,
                                                               zx::eventpair confirm) {
      self->OnIndicateValue(internal_id, std::move(update), std::move(confirm));
    };
    self->services_.emplace(internal_id, Service{.local_svc_ptr = std::move(service_ptr)});
    self->service_id_mapping_[client_svc_id] = internal_id;
    callback(fpromise::ok());
  };

  gatt()->RegisterService(
      std::move(gatt_svc), std::move(id_cb),
      fit::bind_member<&Gatt2ServerServer::OnReadRequest>(this),
      fit::bind_member<&Gatt2ServerServer::OnWriteRequest>(this),
      fit::bind_member<&Gatt2ServerServer::OnClientCharacteristicConfiguration>(this));
}

void Gatt2ServerServer::OnReadRequest(bt::PeerId peer_id, bt::gatt::IdType service_id,
                                      btg::IdType id, uint16_t offset,
                                      btg::ReadResponder responder) {
  auto svc_iter = services_.find(InternalServiceId(service_id));
  // GATT must only send read requests for registered services.
  ZX_ASSERT(svc_iter != services_.end());

  auto cb = [responder = std::move(responder)](LocalService_ReadValue_Result res) mutable {
    if (res.is_err()) {
      responder(fitx::error(fidl_helpers::Gatt2ErrorCodeFromFidl(res.err())), bt::BufferView());
      return;
    }
    responder(fitx::ok(), bt::BufferView(res.response().value));
  };
  svc_iter->second.local_svc_ptr->ReadValue(fbt::PeerId{peer_id.value()}, Handle{id}, offset,
                                            std::move(cb));
}

void Gatt2ServerServer::OnWriteRequest(bt::PeerId peer_id, bt::gatt::IdType service_id,
                                       btg::IdType id, uint16_t offset, const bt::ByteBuffer& value,
                                       btg::WriteResponder responder) {
  auto svc_iter = services_.find(InternalServiceId(service_id));
  // GATT must only send write requests for registered services.
  ZX_ASSERT(svc_iter != services_.end());

  auto cb = [responder = std::move(responder)](LocalService_WriteValue_Result result) mutable {
    // If this was a Write Without Response request, the response callback will be null.
    if (responder) {
      fitx::result<bt::att::ErrorCode> rsp = fitx::ok();
      if (!result.is_response()) {
        rsp = fitx::error(fidl_helpers::Gatt2ErrorCodeFromFidl(result.err()));
      }
      responder(rsp);
    }
  };

  LocalServiceWriteValueRequest params;
  params.set_peer_id(fbt::PeerId{peer_id.value()});
  params.set_handle(Handle{id});
  params.set_offset(offset);
  params.set_value(value.ToVector());
  svc_iter->second.local_svc_ptr->WriteValue(std::move(params), std::move(cb));
}

void Gatt2ServerServer::OnClientCharacteristicConfiguration(bt::gatt::IdType service_id,
                                                            bt::gatt::IdType chrc_id,
                                                            bt::PeerId peer_id, bool notify,
                                                            bool indicate) {
  auto svc_iter = services_.find(InternalServiceId(service_id));
  // GATT must only send CCC updates for registered services.
  ZX_ASSERT(svc_iter != services_.end());

  auto cb = []() { bt_log(TRACE, "fidl", "characteristic configuration acknowledged"); };
  svc_iter->second.local_svc_ptr->CharacteristicConfiguration(
      fbt::PeerId{peer_id.value()}, Handle{chrc_id}, notify, indicate, cb);
}

void Gatt2ServerServer::OnSuppressDiscovery(InternalServiceId service_id) {
  // TODO(fxbug.dev/98598): This event is not yet supported
  bt_log(ERROR, "fidl", "%s not supported - see fxbug.dev/98598", __FUNCTION__);
}

bool Gatt2ServerServer::ValidateValueChangedEvent(
    InternalServiceId service_id, const fuchsia::bluetooth::gatt2::ValueChangedParameters& update,
    const char* update_type) {
  auto iter = services_.find(service_id);
  // It is impossible for clients to send events to a closed service.
  ZX_ASSERT(iter != services_.end());
  // Subtract credit before validating parameters so that credits aren't permanently lost from the
  // client's perspective.
  SubtractCredit(iter->second);

  if (!update.has_handle()) {
    bt_log(WARN, "fidl", "ValueChangedParameters missing required `handle`");
    return false;
  }
  if (!update.has_value()) {
    bt_log(WARN, "fidl", "ValueChangedParameters missing required `value`");
    return false;
  }
  return true;
}

void Gatt2ServerServer::OnNotifyValue(InternalServiceId service_id,
                                      fuchsia::bluetooth::gatt2::ValueChangedParameters update) {
  if (!ValidateValueChangedEvent(service_id, update, "notification")) {
    RemoveService(service_id);
    return;
  }
  bt::gatt::IndicationCallback indicate_cb = nullptr;
  if (!update.has_peer_ids() || update.peer_ids().empty()) {
    gatt()->UpdateConnectedPeers(service_id.value(), update.handle().value, update.value(),
                                 /*indicate_cb=*/nullptr);
    return;
  }
  for (auto peer_id : update.peer_ids()) {
    gatt()->SendUpdate(service_id.value(), update.handle().value, bt::PeerId(peer_id.value),
                       update.value(),
                       /*indicate_cb=*/nullptr);
  }
}

void Gatt2ServerServer::OnIndicateValue(InternalServiceId service_id,
                                        fuchsia::bluetooth::gatt2::ValueChangedParameters update,
                                        zx::eventpair confirmation) {
  if (!ValidateValueChangedEvent(service_id, update, "indication")) {
    RemoveService(service_id);
    return;
  }

  if (!update.has_peer_ids() || update.peer_ids().empty()) {
    auto indicate_cb = [confirm = std::move(confirmation)](bt::att::Result<> status) mutable {
      if (status.is_error()) {
        bt_log(WARN, "fidl", "indication failed: %s", bt_str(status));
        return;
      }
      confirm.signal_peer(/*clear_mask=*/0, ZX_EVENTPAIR_SIGNALED);
    };
    gatt()->UpdateConnectedPeers(service_id.value(), update.handle().value, update.value(),
                                 std::move(indicate_cb));
    return;
  }

  bt::att::ResultFunction<> shared_result_fn =
      [pending = update.peer_ids().size(),
       confirm = std::move(confirmation)](bt::att::Result<> res) mutable {
        if (!confirm.is_valid()) {
          // An error was already signaled.
          return;
        }
        if (res.is_error()) {
          bt_log(INFO, "fidl", "failed to indicate some peers: %s", bt_str(res.error_value()));
          confirm.reset();  // signals ZX_EVENTPAIR_PEER_CLOSED
          return;
        }
        pending--;
        if (pending == 0) {
          confirm.signal_peer(/*clear_mask=*/0, ZX_EVENTPAIR_SIGNALED);
        }
      };

  for (auto peer_id : update.peer_ids()) {
    gatt()->SendUpdate(service_id.value(), update.handle().value, bt::PeerId(peer_id.value),
                       update.value(), shared_result_fn.share());
  }
}

void Gatt2ServerServer::SubtractCredit(Service& svc) {
  // It is impossible for clients to violate the credit system from the server's
  // perspective because new credits are granted before the count reaches 0 (excessive events will
  // fill the FIDL channel and eventually crash the client).
  ZX_ASSERT(svc.credits > 0);
  svc.credits--;
  if (svc.credits <= REFRESH_CREDITS_AT) {
    // Static cast OK because current_credits > 0 and we never add more than
    // INITIAL_VALUE_CHANGED_CREDITS.
    uint8_t credits_to_add = static_cast<uint8_t>(INITIAL_VALUE_CHANGED_CREDITS - svc.credits);
    svc.local_svc_ptr->ValueChangedCredit(credits_to_add);
    svc.credits = INITIAL_VALUE_CHANGED_CREDITS;
  }
}

}  // namespace bthost
