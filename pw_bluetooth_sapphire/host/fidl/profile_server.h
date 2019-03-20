// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_PROFILE_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_PROFILE_SERVER_H_

#include <fuchsia/bluetooth/bredr/cpp/fidl.h>
#include "lib/fidl/cpp/binding.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/weak_ptr.h"

#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/service_record.h"

namespace bthost {

// Implements the bredr::Profile FIDL interface.
class ProfileServer
    : public AdapterServerBase<fuchsia::bluetooth::bredr::Profile> {
 public:
  ProfileServer(
      fxl::WeakPtr<bt::gap::Adapter> adapter,
      fidl::InterfaceRequest<fuchsia::bluetooth::bredr::Profile> request);
  ~ProfileServer() override;

 private:
  // fuchsia::bluetooth::bredr::Profile overrides:
  void AddService(fuchsia::bluetooth::bredr::ServiceDefinition definition,
                  fuchsia::bluetooth::bredr::SecurityLevel sec_level,
                  bool devices, AddServiceCallback callback) override;
  void RemoveService(uint64_t service_id) override;
  void ConnectL2cap(std::string remote_id, uint16_t psm,
                    ConnectL2capCallback callback) override;

  // Callback for incoming connections
  void OnChannelConnected(uint64_t service_id, zx::socket connection,
                          bt::hci::ConnectionHandle handle,
                          const bt::sdp::DataElement& protocol_list);

  // Registered service IDs handed out, correlated with Service Handles.
  std::map<uint64_t, bt::sdp::ServiceHandle> registered_;

  // Last service ID handed out
  uint64_t last_service_id_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<ProfileServer> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(ProfileServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_PROFILE_SERVER_H_
