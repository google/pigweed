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
"""Temporary constant definitions."""

import enum

# TODO: pwbug.dev/366316523 - These constants should all come from the generated
# FileDescriptorProto, which requires upgrading our Protobuf libraries.
# Remove this file once the upgrade is complete.

# From the CodeGeneratorResponse message, indicating that a generator plugin
# supports Protobuf editions.
#
# https://cs.opensource.google/protobuf/protobuf/+/main:src/google/protobuf/compiler/plugin.proto;l=102;drc=cba41100fb1e998660ef4fb12aeef2871a15427c
FEATURE_SUPPORTS_EDITIONS = 2


# Field presence enum from the FeatureSet message.
# https://cs.opensource.google/protobuf/protobuf/+/main:src/google/protobuf/descriptor.proto;l=965;drc=5a68dddcf9564f92815296099f07f7dfe8713908
class FieldPresence(enum.Enum):
    FIELD_PRESENCE_UNKNOWN = 0
    EXPLICIT = 1
    IMPLICIT = 2
    LEGACY_REQUIRED = 3


# Edition enum from the descriptor proto.
# https://cs.opensource.google/protobuf/protobuf/+/main:src/google/protobuf/descriptor.proto;l=68;drc=9d7236b421634afcecc24b6fd63da0ff5b506394
class Edition(enum.Enum):
    EDITION_UNKNOWN = 0
    EDITION_LEGACY = 900
    EDITION_PROTO2 = 998
    EDITION_PROTO3 = 999
    EDITION_2023 = 1000
