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
