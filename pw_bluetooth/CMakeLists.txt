# Copyright 2022 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(pw_bluetooth INTERFACE
  HEADERS
    public/pw_bluetooth/address.h
    public/pw_bluetooth/assigned_uuids.h
    public/pw_bluetooth/gatt/client.h
    public/pw_bluetooth/gatt/constants.h
    public/pw_bluetooth/gatt/error.h
    public/pw_bluetooth/gatt/server.h
    public/pw_bluetooth/gatt/types.h
    public/pw_bluetooth/internal/hex.h
    public/pw_bluetooth/internal/raii_ptr.h
    public/pw_bluetooth/low_energy/advertising_data.h
    public/pw_bluetooth/low_energy/bond_data.h
    public/pw_bluetooth/low_energy/central.h
    public/pw_bluetooth/low_energy/connection.h
    public/pw_bluetooth/low_energy/peripheral.h
    public/pw_bluetooth/low_energy/security_mode.h
    public/pw_bluetooth/constants.h
    public/pw_bluetooth/controller.h
    public/pw_bluetooth/host.h
    public/pw_bluetooth/pairing_delegate.h
    public/pw_bluetooth/peer.h
    public/pw_bluetooth/result.h
    public/pw_bluetooth/types.h
    public/pw_bluetooth/uuid.h
    public/pw_bluetooth/vendor.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers
    pw_function
    pw_status
    pw_string.string
    pw_chrono.system_clock
)

pw_add_test(pw_bluetooth.address_test
  SOURCES
    address_test.cc
  PRIVATE_DEPS
    pw_bluetooth
  GROUPS
    pw_bluetooth
)

pw_add_test(pw_bluetooth.api_test
  SOURCES
    api_test.cc
  PRIVATE_DEPS
    pw_bluetooth
  GROUPS
    pw_bluetooth
)

pw_add_test(pw_bluetooth.result_test
  SOURCES
    result_test.cc
  PRIVATE_DEPS
    pw_bluetooth
  GROUPS
    pw_bluetooth
)

pw_add_test(pw_bluetooth.uuid_test
  SOURCES
    uuid_test.cc
  PRIVATE_DEPS
    pw_bluetooth
  GROUPS
    pw_bluetooth
)

###############################################################################
##          Everything below here is intended to be emboss only              ##
##          and will be skipped if emboss isn't enabled.                     ##
###############################################################################
if("${dir_pw_third_party_emboss}" STREQUAL "")
  # Skip emboss defs if it's not configured
  return()
endif()

include($ENV{PW_ROOT}/third_party/emboss/emboss.cmake)

emboss_cc_library(pw_bluetooth.hci_common
  SOURCES
    public/pw_bluetooth/hci_common.emb
)

emboss_cc_library(pw_bluetooth.hci_commands
  SOURCES
    public/pw_bluetooth/hci_commands.emb
  IMPORT_DIRS
    public
  DEPS
    pw_bluetooth.hci_common
)

emboss_cc_library(pw_bluetooth.hci_events
  SOURCES
    public/pw_bluetooth/hci_events.emb
  IMPORT_DIRS
    public
  DEPS
    pw_bluetooth.hci_common
)

emboss_cc_library(pw_bluetooth.hci_h4
  SOURCES
    public/pw_bluetooth/hci_h4.emb
  IMPORT_DIRS
    public
)

emboss_cc_library(pw_bluetooth.hci_test
  SOURCES
    public/pw_bluetooth/hci_test.emb
  IMPORT_DIRS
    public
  DEPS
    pw_bluetooth.hci_common
    pw_bluetooth.l2cap_frames
)

emboss_cc_library(pw_bluetooth.hci_vendor
  SOURCES
    public/pw_bluetooth/hci_vendor.emb
  IMPORT_DIRS
    public
  DEPS
    pw_bluetooth.hci_common
)

emboss_cc_library(pw_bluetooth.hci_data
  SOURCES
    public/pw_bluetooth/hci_data.emb
)

emboss_cc_library(pw_bluetooth.l2cap_frames
  SOURCES
    public/pw_bluetooth/l2cap_frames.emb
)

pw_add_library("pw_bluetooth.hci_group" INTERFACE
  PUBLIC_DEPS
    pw_bluetooth.hci_commands
    pw_bluetooth.hci_common
    pw_bluetooth.hci_data
    pw_bluetooth.hci_events
    pw_bluetooth.hci_h4
    pw_bluetooth.hci_vendor
    )

pw_target_link_targets("pw_bluetooth._public_config"
  INTERFACE
    pw_bluetooth.hci_commands
    pw_bluetooth.hci_common
    pw_bluetooth.hci_data
    pw_bluetooth.hci_events
    pw_bluetooth.hci_h4
    pw_bluetooth.hci_vendor
    pw_bluetooth.l2cap_frames
)

pw_add_test(pw_bluetooth.emboss_test
  SOURCES
    emboss_test.cc
  PRIVATE_DEPS
    pw_bluetooth.hci_group
    pw_bluetooth.hci_test
    pw_bluetooth.l2cap_frames
  GROUPS
    pw_bluetooth
)
