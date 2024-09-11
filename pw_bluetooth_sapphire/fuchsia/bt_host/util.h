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

#include <fidl/fuchsia.hardware.bluetooth/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <pw_async_fuchsia/dispatcher.h>

#include <string>

namespace bthost {

// Creates a FIDL channel connecting to the service directory at |device_path|
// relative to component's namespace. Creates and returns a VendorHandle using
// the client end of the channel if successful, otherwise returns nullptr on
// failure.
zx::result<fidl::ClientEnd<fuchsia_hardware_bluetooth::Vendor>>
CreateVendorHandle(const std::string& device_path);

}  // namespace bthost
