// Copyright 2023 The Pigweed Authors
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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_LINK_TYPE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_LINK_TYPE_H_

#include <string>

namespace bt {

// This defines the various connection types. These do not exactly correspond
// to the baseband logical/physical link types but instead provide a
// high-level abstraction.
enum class LinkType {
  // Represents a BR/EDR baseband link (ACL-U).
  kACL,

  // BR/EDR synchronous links (SCO-S, eSCO-S).
  kSCO,
  kESCO,

  // A LE logical link (LE-U).
  kLE,
};
std::string LinkTypeToString(LinkType type);

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_LINK_TYPE_H_
