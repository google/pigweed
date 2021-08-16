# Copyright 2021 The Pigweed Authors
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
"""Implements the set of dependencies that bazel-embedded requires."""

def _toolchain_upstream_repository_impl(rctx):
    """Creates a remote repository with a set of toolchain components.

    The bazel embedded toolchain expects two targets injected_headers
    and polyfill. This is rule generates these targets so that
    bazel-embedded can depend on them and so that the targets can depend
    on pigweeds implementation of polyfill. This rule is only ever
    intended to be instantiated with the name
    "bazel_embedded_upstream_toolchain", and should only be used from
    the "toolchain_upstream_deps" macro.

    The bazel-embedded package expects to be able to access these
    targets as @bazel_embedded_upstream_toolchain//:polyfill and
    @bazel_embedded_upstream_toolchain//:injected_headers.

    Args:
      rctx: Repository context.
    """
    rctx.file("BUILD", """
package(default_visibility = ["//visibility:public"])
load(
    "@bazel_embedded//toolchains/tools/include_tools:defs.bzl",
    "cc_injected_toolchain_header_library",
    "cc_polyfill_toolchain_library",
)

cc_polyfill_toolchain_library(
    name = "polyfill",
    deps = ["@pigweed//pw_polyfill:toolchain_polyfill_overrides"],
)

cc_injected_toolchain_header_library(
    name = "injected_headers",
    deps = ["@pigweed//pw_polyfill:toolchain_injected_headers"],
)
""")

_toolchain_upstream_repository = repository_rule(
    _toolchain_upstream_repository_impl,
    doc = """
toolchain_upstream_repository creates a remote repository that can be
accessed by a toolchain repository to configure system includes.

It's recommended to use this rule through the 'toolchain_upstream_deps'
macro rather than using this rule directly.
""",
)

def toolchain_upstream_deps():
    """Implements the set of dependencies that bazel-embedded requires.

    These targets are used to override the default toolchain
    requirements in the remote bazel-embedded toolchain. The remote
    toolchain expects to find two targets;
      - "@bazel_embedded_upstream_toolchain//:polyfill" -> Additional
        system headers for the toolchain
      - "@bazel_embedded_upstream_toolchain//:injected_headers" ->
        Headers that are injected into the toolchain via the -include
        command line argument
    """
    _toolchain_upstream_repository(
        name = "bazel_embedded_upstream_toolchain",
    )
