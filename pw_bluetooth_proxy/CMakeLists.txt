# Copyright 2024 The Pigweed Authors
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

###############################################################################
##          Everything below here is intended to be emboss only              ##
##          and will be skipped if emboss isn't enabled.                     ##
###############################################################################
if("${dir_pw_third_party_emboss}" STREQUAL "")
  # Skip emboss defs if it's not configured
  return()
endif()

pw_add_library(pw_bluetooth_proxy STATIC
  HEADERS
    public/pw_bluetooth_proxy/acl_data_channel.h
    public/pw_bluetooth_proxy/common.h
    public/pw_bluetooth_proxy/proxy_host.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bluetooth.emboss_hci_common
    pw_bluetooth.emboss_hci_events
    pw_bluetooth.emboss_hci_h4
    pw_function
    pw_log
    pw_span
  SOURCES
    acl_data_channel.cc
    emboss_util.h
    proxy_host.cc
)

pw_add_test(pw_bluetooth_proxy.emboss_util_test
  SOURCES
    emboss_util.h
    emboss_util_test.cc
  PRIVATE_DEPS
    pw_bluetooth_proxy
    pw_bluetooth.emboss_hci_test
    pw_span
  GROUPS
    modules
)

pw_add_test(pw_bluetooth_proxy.pw_bluetooth_proxy_test
  SOURCES
    emboss_util.h
    proxy_host_test.cc
  PRIVATE_DEPS
    pw_bluetooth_proxy
    pw_third_party.fuchsia.stdcompat
    pw_bluetooth.emboss_hci_commands
    pw_bluetooth.emboss_hci_common
    pw_bluetooth.emboss_hci_events
    pw_bluetooth.emboss_hci_h4
  GROUPS
    modules
)
