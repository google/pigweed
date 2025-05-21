// Copyright 2025 The Pigweed Authors
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

namespace pw::bluetooth::proxy {

/// ChannelProxy allows a client to write, read, and receive events from an
/// underlying Bluetooth channel.
class ChannelProxy {
 public:
  explicit ChannelProxy() {}
  virtual ~ChannelProxy() = default;

  ChannelProxy(const ChannelProxy& other) = delete;
  ChannelProxy& operator=(const ChannelProxy& other) = delete;
  ChannelProxy(ChannelProxy&& other) = default;
  ChannelProxy& operator=(ChannelProxy&& other) = default;
};

}  // namespace pw::bluetooth::proxy
