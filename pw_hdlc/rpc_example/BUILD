# Copyright 2020 The Pigweed Authors
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

load(
    "//pw_build:pigweed.bzl",
    "pw_cc_library",
)

pw_cc_library(
    name = "rpc_example",
    srcs = [
        "hdlc_rpc_server.cc",
        "main.cc",
    ],
    hdrs = [
        "public/pw_hdlc/decoder.h",
        "public/pw_hdlc/hdlc_channel.h",
        "public/pw_hdlc/rpc_server_packets.h",
    ],
    deps = [
        "//pw_hdlc",
        "//pw_hdlc:pw_rpc",
        "//pw_rpc:server",
        "//pw_log",
    ],
)

