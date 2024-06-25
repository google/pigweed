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

    # This repository is pinned to the upstream `develop` branch of the Pico SDK.
    maybe(
        git_repository,
        name = "pico-sdk",
        commit = "6ff3e4fab27441de19fd53c0eb5aacbe83a18221",
        remote = "https://pigweed.googlesource.com/third_party/github/raspberrypi/pico-sdk",
    )

    # TODO: https://pwbug.dev/345244650 - Upstream bazel build.
    maybe(
        git_repository,
        name = "picotool",
        commit = "49072f6ebbc814dcc74d6f8b753b89c24af12971",
        remote = "https://github.com/armandomontanez/picotool",
    )

    # ---- probe-rs Paths ----
    #
    # NOTE: These paths and sha-s have been manually copied from
    # https://github.com/probe-rs/probe-rs/releases/tag/v0.24.0
    maybe(
        http_archive,
        name = "probe-rs-tools-x86_64-unknown-linux-gnu",
        build_file = "@pigweed//third_party/probe-rs:probe-rs.BUILD.bazel",
        sha256 = "21e8d7df39fa0cdc9a0421e0ac2ac5ba81ec295ea11306f26846089f6fe975c0",
        strip_prefix = "probe-rs-tools-x86_64-unknown-linux-gnu",
        url = "https://github.com/probe-rs/probe-rs/releases/download/v0.24.0/probe-rs-tools-x86_64-unknown-linux-gnu.tar.xz",
    )
    maybe(
        http_archive,
        name = "probe-rs-tools-aarch64-unknown-linux-gnu",
        build_file = "@pigweed//third_party/probe-rs:probe-rs.BUILD.bazel",
        sha256 = "95d91ebe08868d5119a698e3268ff60a4d71d72afa26ab207d43c807c729c90a",
        strip_prefix = "probe-rs-tools-aarch64-unknown-linux-gnu",
        url = "https://github.com/probe-rs/probe-rs/releases/download/v0.24.0/probe-rs-tools-aarch64-unknown-linux-gnu.tar.xz",
    )
    maybe(
        http_archive,
        name = "probe-rs-tools-x86_64-apple-darwin",
        build_file = "@pigweed//third_party/probe-rs:probe-rs.BUILD.bazel",
        sha256 = "0e35cc92ff34af1b1c72dd444e6ddd57c039ed31c2987e37578864211e843cf1",
        strip_prefix = "probe-rs-tools-x86_64-apple-darwin",
        url = "https://github.com/probe-rs/probe-rs/releases/download/v0.24.0/probe-rs-tools-x86_64-apple-darwin.tar.xz",
    )
    maybe(
        http_archive,
        name = "probe-rs-tools-aarch64-apple-darwin",
        build_file = "@pigweed//third_party/probe-rs:probe-rs.BUILD.bazel",
        sha256 = "7140d9c2c61f8712ba15887f74df0cb40a7b16728ec86d5f45cc93fe96a0a29a",
        strip_prefix = "probe-rs-tools-aarch64-apple-darwin",
        url = "https://github.com/probe-rs/probe-rs/releases/download/v0.24.0/probe-rs-tools-aarch64-apple-darwin.tar.xz",
    )
    maybe(
        http_archive,
        name = "probe-rs-tools-x86_64-pc-windows-msvc",
        build_file = "@pigweed//third_party/probe-rs:probe-rs.BUILD.bazel",
        sha256 = "d195dfa3466a87906251e27d6d70a0105274faa28ebf90ffadad0bdd89b1ec77",
        strip_prefix = "probe-rs-tools-x86_64-pc-windows-msvc",
        url = "https://github.com/probe-rs/probe-rs/releases/download/v0.24.0/probe-rs-tools-x86_64-pc-windows-msvc.zip",
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
