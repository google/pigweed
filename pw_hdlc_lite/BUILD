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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_library(
    name = "pw_hdlc_lite",
    srcs = [
        "decoder.cc",
        "encoder.cc",
        "pw_hdlc_lite_private/protocol.h",
        "rpc_packets.cc",
    ],
    hdrs = [
        "public/pw_hdlc_lite/decoder.h",
        "public/pw_hdlc_lite/encoder.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_bytes",
        "//pw_checksum",
        "//pw_log",
        "//pw_result",
        "//pw_span",
        "//pw_status",
        "//pw_stream",
    ],
)

pw_cc_library(
    name = "pw_rpc",
    srcs = ["rpc_packets.cc"],
    hdrs = [
        "public/pw_hdlc_lite/rpc_channel.h",
        "public/pw_hdlc_lite/rpc_packets.h",
    ],
    includes = ["public"],
    deps = [
        ":pw_hdlc_lite",
        "//pw_rpc:server",
    ],
)

cc_test(
    name = "encoder_test",
    srcs = ["encoder_test.cc"],
    deps = [
        ":pw_hdlc_lite",
        "//pw_stream",
        "//pw_unit_test",
    ],
)

cc_test(
    name = "decoder_test",
    srcs = ["decoder_test.cc"],
    deps = [
        ":pw_hdlc_lite",
        "//pw_result",
        "//pw_stream",
        "//pw_unit_test",
    ],
)

cc_test(
    name = "rpc_channel_test",
    srcs = ["rpc_channel_test.cc"],
    deps = [
        ":pw_hdlc_lite",
        "//pw_stream",
        "//pw_unit_test",
    ],
)
