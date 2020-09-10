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

package(default_visibility = ["//visibility:public"])

load("@npm//@bazel/typescript:index.bzl", "ts_library", "ts_devserver")
load("//pw_web_ui:web_bundle.bzl", "web_bundle")

ts_library(
    name = "app_lib",
    srcs = [
        "app.tsx",
        "index.tsx",
    ],
    deps = [
        "@npm//@types/react",
        "@npm//react",
        "@npm//@types/react-dom",
        "@npm//react-dom",
        "@npm//@material-ui/core",
        "//pw_web_ui/src/transport:web_serial_transport_lib",
    ],
)

web_bundle(
    name = "app_bundle",
    deps = [
        ":app_lib",
    ],
    entry_point = "index.tsx",
)

ts_devserver(
    # TODO(msoulanille): Use the devserver's bundler
    # instead of serving the production bundle.
    name = "devserver",
    static_files = [
        "index.html",
        ":app_bundle",
    ]
)

ts_devserver(
    # Bundles and serves the production bundle for testing.
    # Should NOT be used for serving in production.
    name = "prodserver",
    static_files = [
        "index.html",
        ":app_bundle",
    ]
)
