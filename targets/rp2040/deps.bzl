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

"""Contains the external dependencies for rp2040."""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def pigweed_rp2_deps():
    """Instantiates the dependencies of the rp2040 rules."""
    maybe(
        http_archive,
        name = "platforms",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz",
        ],
        sha256 = "8150406605389ececb6da07cbcb509d5637a3ab9a24bc69b1101531367d89d74",
    )
    maybe(
        http_archive,
        name = "rules_platform",
        sha256 = "0aadd1bd350091aa1f9b6f2fbcac8cd98201476289454e475b28801ecf85d3fd",
        url = "https://github.com/bazelbuild/rules_platform/releases/download/0.1.0/rules_platform-0.1.0.tar.gz",
    )

    # Pinned to 2.0.0 releases.
    maybe(
        http_archive,
        name = "pico-sdk",
        sha256 = "626db87779fa37f7f9c7cfe3e152f7e828fe19c486730af2e7c80836b6b57e1d",
        url = "https://github.com/raspberrypi/pico-sdk/releases/download/2.0.0/pico-sdk-2.0.0.tar.gz",
    )

    maybe(
        http_archive,
        name = "picotool",
        sha256 = "9392c4a31f16b80b70f861c37a029701d3212e212840daa097c8a3720282ce65",
        url = "https://github.com/raspberrypi/picotool/releases/download/2.0.0/picotool-2.0.0.tar.gz",
    )

    maybe(
        git_repository,
        name = "rules_libusb",
        commit = "eb8c39291104b08d5a2ea69d31d79c61a85a8485",
        remote = "https://pigweed.googlesource.com/pigweed/rules_libusb",
    )
    maybe(
        http_archive,
        name = "libusb",
        build_file = "@rules_libusb//:libusb.BUILD",
        sha256 = "ffaa41d741a8a3bee244ac8e54a72ea05bf2879663c098c82fc5757853441575",
        strip_prefix = "libusb-1.0.27",
        url = "https://github.com/libusb/libusb/releases/download/v1.0.27/libusb-1.0.27.tar.bz2",
    )
    maybe(
        http_archive,
        name = "tinyusb",
        build_file = "@pico-sdk//src/rp2_common/tinyusb:tinyusb.BUILD",
        sha256 = "ac57109bba00d26ffa33312d5f334990ec9a9a4d82bf890ed8b825b4610d1da2",
        strip_prefix = "tinyusb-86c416d4c0fb38432460b3e11b08b9de76941bf5",
        url = "https://github.com/hathach/tinyusb/archive/86c416d4c0fb38432460b3e11b08b9de76941bf5.zip",
    )
