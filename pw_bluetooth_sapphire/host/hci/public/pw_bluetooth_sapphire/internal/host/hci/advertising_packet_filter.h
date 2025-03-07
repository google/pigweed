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

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
namespace bt::hci {

class AdvertisingPacketFilter {
 public:
  // Some Controllers support offloaded packet filtering (e.g. in Android vendor
  // extensions). PacketFilterConfig allows us to switch on whether this feature
  // is enabled for the current Controller or not.
  class Config {
   public:
    Config(bool offloading_enabled, uint8_t max_filters)
        : offloading_enabled_(offloading_enabled), max_filters_(max_filters) {}

    bool offloading_enabled() const { return offloading_enabled_; }
    uint8_t max_filters() const { return max_filters_; }

   private:
    bool offloading_enabled_ = false;
    uint8_t max_filters_ = 0;
  };

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisingPacketFilter);
};

}  // namespace bt::hci
