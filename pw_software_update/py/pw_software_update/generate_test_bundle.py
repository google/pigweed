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
"""Script for generating test bundles"""

import argparse
import subprocess
import sys
from typing import Dict

from pw_software_update import tuf_pb2, metadata, update_bundle_pb2

HEADER = """// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include "pw_bytes/span.h"

"""


def byte_array_declaration(data: bytes, name: str) -> str:
    """Generates a byte C array declaration for a byte array"""
    type_name = '[[maybe_unused]] const std::byte'
    byte_str = ''.join([f'std::byte{{0x{b:02x}}},' for b in data])
    array_body = f'{{{byte_str}}}'
    return f'{type_name} {name}[] = {array_body};'


def proto_array_declaration(proto, name: str) -> str:
    """Generates a byte array declaration for a proto"""
    return byte_array_declaration(proto.SerializeToString(), name)


def sign_metadata(role: str, metadata_proto):
    """Signs a serialized metadata for a given role"""
    signed = getattr(tuf_pb2, f'Signed{role.capitalize()}Metadata')()
    serialized = metadata_proto.SerializeToString()
    setattr(signed, f'serialized_{role}_metadata', serialized)
    # TODO(pwbug:456): Generate signatures.

    return signed


class Bundle:
    """A helper for test UpdateBundle generation"""
    def __init__(self):
        self._payloads: Dict[str, bytes] = {}

    def add_payload(self, name: str, payload: bytes) -> None:
        """Adds a payload to the bundle"""
        self._payloads[name] = payload

    def generate_targets_metadata(self) -> tuf_pb2.TargetsMetadata:
        """Generates the targets metadata"""
        targets = metadata.gen_targets_metadata(self._payloads)
        return targets

    def generate_signed_metadta(self, role: str):
        """Generate a signed metadata of a given role"""
        unsigned = getattr(self, f'generate_{role}_metadata')()
        return sign_metadata(role, unsigned)

    def generate_bundle(self) -> update_bundle_pb2.UpdateBundle:
        """Generate the update bundle"""
        bundle = update_bundle_pb2.UpdateBundle()
        bundle.targets_metadata['targets'].CopyFrom(
            self.generate_signed_metadta('targets'))
        for name, payload in self._payloads.items():
            bundle.target_payloads[name] = payload

        return bundle

    def generate_manifest(self) -> update_bundle_pb2.Manifest:
        """Generates the manifest"""
        manifest = update_bundle_pb2.Manifest()
        manifest.targets_metadata['targets'].CopyFrom(
            self.generate_targets_metadata())
        return manifest


def parse_args():
    """Setup argparse."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_header",
                        help="output path of the generated C header")
    return parser.parse_args()


def main() -> int:
    """Main"""
    args = parse_args()

    test_bundle = Bundle()

    # Add some update files.
    test_bundle.add_payload('file1', 'file 1 content'.encode())  # type: ignore
    test_bundle.add_payload('file2', 'file 2 content'.encode())  # type: ignore

    update_bundle_proto = test_bundle.generate_bundle()
    manifest_proto = test_bundle.generate_manifest()

    with open(args.output_header, 'w') as header:
        header.write(HEADER)
        header.write('namespace {')
        header.write(
            proto_array_declaration(update_bundle_proto, 'kTestBundle'))
        header.write(
            proto_array_declaration(manifest_proto, 'kTestBundleManifest'))
        header.write('}')

    subprocess.run([
        'clang-format',
        '-i',
        args.output_header,
    ], check=True)

    return 0


if __name__ == "__main__":
    sys.exit(main())
