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
"""Old home of proto compilation rules.

This file is DEPRECATED. Prefer to load the specific rules you need from the
other .bzl files in this directory.
"""

load("//pw_protobuf_compiler:nanopb_proto_library.bzl", _nanopb_proto_library = "nanopb_proto_library")
load("//pw_protobuf_compiler:nanopb_rpc_proto_library.bzl", _nanopb_rpc_proto_library = "nanopb_rpc_proto_library")
load("//pw_protobuf_compiler:pw_proto_filegroup.bzl", _pw_proto_filegroup = "pw_proto_filegroup")
load("//pw_protobuf_compiler:pwpb_proto_library.bzl", _pwpb_proto_library = "pwpb_proto_library")
load("//pw_protobuf_compiler:pwpb_rpc_proto_library.bzl", _pwpb_rpc_proto_library = "pwpb_rpc_proto_library")
load("//pw_protobuf_compiler:raw_rpc_proto_library.bzl", _raw_rpc_proto_library = "raw_rpc_proto_library")

# These rules are re-exported here for backwards compatibility, but in new code
# prefer loading them directly from the corresponding bzl file.
pw_proto_filegroup = _pw_proto_filegroup
pwpb_proto_library = _pwpb_proto_library
pwpb_rpc_proto_library = _pwpb_rpc_proto_library
raw_rpc_proto_library = _raw_rpc_proto_library
nanopb_proto_library = _nanopb_proto_library
nanopb_rpc_proto_library = _nanopb_rpc_proto_library

def pw_proto_library(
        *,
        name,
        deps,
        enabled_targets = None,
        **kwargs):
    """Generate Pigweed proto C++ code.

    DEPRECATED. This macro is deprecated and will be removed in a future
    Pigweed version. Please use the single-target macros above.

    Args:
      name: The name of the target.
      deps: proto_library targets from which to generate Pigweed C++.
      enabled_targets: Specifies which libraries should be generated. Libraries
        will only be generated as needed, but unnecessary outputs may conflict
        with other build rules and thus cause build failures. This filter allows
        manual selection of which libraries should be supported by this build
        target in order to prevent such conflicts. The argument, if provided,
        should be a subset of ["pwpb", "nanopb", "raw_rpc", "nanopb_rpc"]. All
        are enabled by default. Note that "nanopb_rpc" relies on "nanopb".
      **kwargs: Passed on to all proto generation rules.

    Example usage:

      proto_library(
        name = "benchmark_proto",
        srcs = [
          "benchmark.proto",
        ],
      )

      pw_proto_library(
        name = "benchmark_pw_proto",
        deps = [":benchmark_proto"],
      )

      pw_cc_binary(
        name = "proto_user",
        srcs = ["proto_user.cc"],
        deps = [":benchmark_pw_proto.pwpb"],
      )

    The pw_proto_library generates the following targets in this example:

    "benchmark_pw_proto.pwpb": C++ library exposing the "benchmark.pwpb.h" header.
    "benchmark_pw_proto.pwpb_rpc": C++ library exposing the
        "benchmark.rpc.pwpb.h" header.
    "benchmark_pw_proto.raw_rpc": C++ library exposing the "benchmark.raw_rpc.h"
        header.
    "benchmark_pw_proto.nanopb": C++ library exposing the "benchmark.pb.h"
        header.
    "benchmark_pw_proto.nanopb_rpc": C++ library exposing the
        "benchmark.rpc.pb.h" header.
    """

    def is_plugin_enabled(plugin):
        return (enabled_targets == None or plugin in enabled_targets)

    if is_plugin_enabled("nanopb"):
        # Use nanopb to generate the pb.h and pb.c files, and the target
        # exposing them.
        nanopb_proto_library(
            name = name + ".nanopb",
            deps = deps,
            **kwargs
        )

    if is_plugin_enabled("pwpb"):
        pwpb_proto_library(
            name = name + ".pwpb",
            deps = deps,
            **kwargs
        )

    if is_plugin_enabled("pwpb_rpc"):
        pwpb_rpc_proto_library(
            name = name + ".pwpb_rpc",
            deps = deps,
            pwpb_proto_library_deps = [":" + name + ".pwpb"],
            **kwargs
        )

    if is_plugin_enabled("raw_rpc"):
        raw_rpc_proto_library(
            name = name + ".raw_rpc",
            deps = deps,
            **kwargs
        )

    if is_plugin_enabled("nanopb_rpc"):
        nanopb_rpc_proto_library(
            name = name + ".nanopb_rpc",
            deps = deps,
            nanopb_proto_library_deps = [":" + name + ".nanopb"],
            **kwargs
        )
