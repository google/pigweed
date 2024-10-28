// Copyright 2024 The Pigweed Authors
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

#pragma once

#include <lib/fit/function.h>

#include <utility>

#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/adapter.h"

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
  ServerBase(Interface* impl, zx::channel channel)
      : binding_(impl, std::move(channel)) {
    PW_DCHECK(binding_.is_bound());
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
  AdapterServerBase(bt::gap::Adapter::WeakPtr adapter,
                    Interface* impl,
                    fidl::InterfaceRequest<Interface> request)
      : AdapterServerBase(adapter, impl, request.TakeChannel()) {}

  AdapterServerBase(bt::gap::Adapter::WeakPtr adapter,
                    Interface* impl,
                    zx::channel channel)
      : ServerBase<Interface>(impl, std::move(channel)),
        adapter_(std::move(adapter)) {
    PW_DCHECK(adapter_.is_alive());
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
  GattServerBase(bt::gatt::GATT::WeakPtr gatt,
                 Interface* impl,
                 fidl::InterfaceRequest<Interface> request)
      : ServerBase<Interface>(impl, std::move(request)),
        gatt_(std::move(gatt)) {
    PW_DCHECK(gatt_.is_alive());
  }

  ~GattServerBase() override = default;

 protected:
  bt::gatt::GATT::WeakPtr gatt() const { return gatt_; }

 private:
  bt::gatt::GATT::WeakPtr gatt_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattServerBase);
};

}  // namespace bthost
