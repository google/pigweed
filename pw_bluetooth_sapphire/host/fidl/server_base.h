// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_SERVER_BASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_SERVER_BASE_H_

#include <lib/fit/function.h>

#include <utility>

#include <fbl/ref_ptr.h>

#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"

namespace bt {

namespace gap {
class Adapter;
}  // namespace gap

namespace gatt {
class GATT;
}  // namespace gatt

}  // namespace bt

namespace bthost {

// This class acts as a common base type for all FIDL interface servers. Its
// main purpose is to provide type erasure for the ServerBase template below.
class Server {
 public:
  virtual ~Server() = default;

  virtual void set_error_handler(fit::function<void(zx_status_t)> handler) = 0;
};

// ServerBase is a common base implementation for FIDL interface servers.
template <typename Interface>
class ServerBase : public Server, public Interface {
 public:
  // Constructs a FIDL server by binding a fidl::InterfaceRequest.
  ServerBase(Interface* impl, fidl::InterfaceRequest<Interface> request)
      : ServerBase(impl, request.TakeChannel()) {}

  // Constructs a FIDL server by binding a zx::channel.
  ServerBase(Interface* impl, zx::channel channel) : binding_(impl, std::move(channel)) {
    BT_DEBUG_ASSERT(binding_.is_bound());
  }

  ~ServerBase() override = default;

  void set_error_handler(fit::function<void(zx_status_t)> handler) override {
    binding_.set_error_handler(std::move(handler));
  }

 protected:
  ::fidl::Binding<Interface>* binding() { return &binding_; }

 private:
  // Holds the channel from the FIDL client.
  ::fidl::Binding<Interface> binding_;

  // Binding cannot be copied or moved.
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(ServerBase);
};

// Base template for GAP FIDL interface servers. The GAP profile is accessible
// through an Adapter object.
template <typename Interface>
class AdapterServerBase : public ServerBase<Interface> {
 public:
  AdapterServerBase(bt::gap::Adapter::WeakPtr adapter, Interface* impl,
                    fidl::InterfaceRequest<Interface> request)
      : AdapterServerBase(adapter, impl, request.TakeChannel()) {}

  AdapterServerBase(bt::gap::Adapter::WeakPtr adapter, Interface* impl, zx::channel channel)
      : ServerBase<Interface>(impl, std::move(channel)), adapter_(std::move(adapter)) {
    BT_DEBUG_ASSERT(adapter_.is_alive());
  }

  ~AdapterServerBase() override = default;

 protected:
  const bt::gap::Adapter::WeakPtr& adapter() const { return adapter_; }

 private:
  bt::gap::Adapter::WeakPtr adapter_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdapterServerBase);
};

// Base template for GATT FIDL interface servers. The GATT profile is accessible
// through an Adapter object.
template <typename Interface>
class GattServerBase : public ServerBase<Interface> {
 public:
  GattServerBase(bt::gatt::GATT::WeakPtr gatt, Interface* impl,
                 fidl::InterfaceRequest<Interface> request)
      : ServerBase<Interface>(impl, std::move(request)), gatt_(std::move(gatt)) {
    BT_DEBUG_ASSERT(gatt_.is_alive());
  }

  ~GattServerBase() override = default;

 protected:
  bt::gatt::GATT::WeakPtr gatt() const { return gatt_; }

 private:
  bt::gatt::GATT::WeakPtr gatt_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattServerBase);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_SERVER_BASE_H_
