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

#include "pw_bluetooth_sapphire/fuchsia/bt_host/host.h"

#include <lib/fdio/directory.h>
#include <lib/inspect/component/cpp/component.h>
#include <pw_assert/check.h>

#include "lib/async/default.h"
#include "pw_bluetooth_sapphire/fuchsia/bt_host/bt_host_config.h"
#include "pw_bluetooth_sapphire/fuchsia/host/controllers/fidl_controller.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/host_server.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/common/random.h"

using namespace bt;

namespace bthost {

BtHostComponent::BtHostComponent(
    async_dispatcher_t* dispatcher,
    const std::string& device_path,
    bool initialize_rng,
    std::unique_ptr<ActivityGovernorLeaseProvider> activity_governor)
    : pw_dispatcher_(dispatcher),
      device_path_(device_path),
      initialize_rng_(initialize_rng),
      inspector_(inspect::ComponentInspector(dispatcher, {})) {
  if (initialize_rng) {
    set_random_generator(&random_generator_);
  }
  inspector_.root().RecordString("name", device_path_);

  if (activity_governor) {
    lease_provider_.emplace<std::unique_ptr<ActivityGovernorLeaseProvider>>(
        std::move(activity_governor));
  }
}

BtHostComponent::~BtHostComponent() {
  if (initialize_rng_) {
    set_random_generator(nullptr);
  }
}

// static
std::unique_ptr<BtHostComponent> BtHostComponent::Create(
    async_dispatcher_t* dispatcher,
    const std::string& device_path,
    std::unique_ptr<ActivityGovernorLeaseProvider> activity_governor) {
  std::unique_ptr<BtHostComponent> host(
      new BtHostComponent(dispatcher,
                          device_path,
                          /*initialize_rng=*/true,
                          std::move(activity_governor)));
  return host;
}

// static
std::unique_ptr<BtHostComponent> BtHostComponent::CreateForTesting(
    async_dispatcher_t* dispatcher, const std::string& device_path) {
  std::unique_ptr<BtHostComponent> host(
      new BtHostComponent(dispatcher,
                          device_path,
                          /*initialize_rng=*/false,
                          /*activity_governor=*/nullptr));
  return host;
}

bool BtHostComponent::Initialize(
    fidl::ClientEnd<fuchsia_hardware_bluetooth::Vendor> vendor_client_end,
    InitCallback init_cb,
    ErrorCallback error_cb,
    bool legacy_pairing_enabled) {
  std::unique_ptr<bt::controllers::FidlController> controller =
      std::make_unique<bt::controllers::FidlController>(
          std::move(vendor_client_end), async_get_default_dispatcher());

  bt_log(INFO, "bt-host", "Create HCI transport layer");
  hci_ = std::make_unique<hci::Transport>(
      std::move(controller), pw_dispatcher_, lease_provider());

  bt_log(INFO, "bt-host", "Create GATT layer");
  gatt_ = gatt::GATT::Create();
  gap::Adapter::Config config = {
      .legacy_pairing_enabled = legacy_pairing_enabled,
  };
  gap_ = gap::Adapter::Create(pw_dispatcher_,
                              hci_->GetWeakPtr(),
                              gatt_->GetWeakPtr(),
                              config,
                              lease_provider());
  if (!gap_) {
    bt_log(WARN, "bt-host", "GAP could not be created");
    return false;
  }
  gap_->AttachInspect(inspector_.root(), "adapter");

  // Called when the GAP layer is ready. We initialize the GATT profile after
  // initial setup in GAP. The data domain will be initialized by GAP because it
  // both sets up the HCI ACL data channel that L2CAP relies on and registers
  // L2CAP services.
  auto gap_init_callback = [callback =
                                std::move(init_cb)](bool success) mutable {
    bt_log(DEBUG,
           "bt-host",
           "GAP init complete status: (%s)",
           (success ? "success" : "failure"));
    callback(success);
  };

  auto transport_closed_callback = [error_cb = std::move(error_cb)]() mutable {
    bt_log(WARN, "bt-host", "HCI transport has closed");
    error_cb();
  };

  bt_log(DEBUG, "bt-host", "Initializing GAP");
  return gap_->Initialize(std::move(gap_init_callback),
                          std::move(transport_closed_callback));
}

void BtHostComponent::ShutDown() {
  bt_log(DEBUG, "bt-host", "Shutting down");

  if (!gap_) {
    bt_log(DEBUG, "bt-host", "Already shut down");
    return;
  }

  // Closes all FIDL channels owned by |host_server_|.
  host_server_ = nullptr;

  // Make sure that |gap_| gets shut down and destroyed on its creation thread
  // as it is not thread-safe.
  gap_->ShutDown();
  gap_ = nullptr;

  // This shuts down the GATT profile and all of its clients.
  gatt_ = nullptr;

  // Shuts down HCI command channel and ACL data channel.
  hci_ = nullptr;
}

void BtHostComponent::BindToHostInterface(
    fidl::ServerEnd<fuchsia_bluetooth_host::Host> host_client,
    uint8_t sco_offload_index) {
  if (host_server_) {
    bt_log(WARN, "bt-host", "Host interface channel already open");
    return;
  }

  PW_DCHECK(gap_);
  PW_DCHECK(gatt_);

  zx::channel channel = host_client.TakeChannel();

  host_server_ = std::make_unique<HostServer>(std::move(channel),
                                              gap_->AsWeakPtr(),
                                              gatt_->GetWeakPtr(),
                                              lease_provider(),
                                              sco_offload_index);
  host_server_->set_error_handler([this](zx_status_t status) {
    PW_DCHECK(host_server_);
    bt_log(WARN, "bt-host", "Host interface disconnected");
    host_server_ = nullptr;
  });
}

pw::bluetooth_sapphire::LeaseProvider& BtHostComponent::lease_provider() {
  pw::bluetooth_sapphire::LeaseProvider* lease_provider = nullptr;
  std::visit(
      [&](auto&& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<
                          T,
                          pw::bluetooth_sapphire::NullLeaseProvider>) {
          lease_provider = &p;
        } else if constexpr (std::is_same_v<
                                 T,
                                 std::unique_ptr<
                                     bthost::ActivityGovernorLeaseProvider>>) {
          lease_provider = p.get();
        } else {
          static_assert(false, "non-exhaustive visitor!");
        }
      },
      lease_provider_);
  return *lease_provider;
}

}  // namespace bthost
