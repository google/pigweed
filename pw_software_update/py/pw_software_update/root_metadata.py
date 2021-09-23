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
"""Facilities for generating the 'root' metadata."""

import argparse
from pathlib import Path

from pw_software_update import keys, metadata
from pw_software_update.tuf_pb2 import (RootMetadata, SignedRootMetadata,
                                        SignatureRequirement)


def gen_root_metadata(root_key_pem: bytes,
                      targets_key_pem: bytes,
                      version: int = 1) -> RootMetadata:
    """Generates a RootMetadata.

    Args:
      root_key_pem: PEM bytes of the root public key.
      targets_key_pem: PEM bytes of the targets public key.
      version: Version number for rollback checks.
    """
    commmon = metadata.gen_commmon_metadata()
    commmon.version = version

    key1 = keys.import_ecdsa_public_key(root_key_pem)
    key2 = keys.import_ecdsa_public_key(targets_key_pem)

    return RootMetadata(common_metadata=commmon,
                        consistent_snapshot=False,
                        keys=[key1, key2],
                        root_signature_requirement=SignatureRequirement(
                            key_ids=[key1.key_id], threshold=1),
                        targets_signature_requirement=SignatureRequirement(
                            key_ids=[key2.key_id], threshold=1))


def parse_args():
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('-o',
                        '--out',
                        type=Path,
                        required=True,
                        help='Output path for the generated root metadata')

    parser.add_argument('--version',
                        type=int,
                        default=1,
                        help='Canonical version number for rollback checks')

    parser.add_argument('--root-key',
                        type=Path,
                        required=True,
                        help='Public key filename for the "Root" role')

    parser.add_argument('--targets-key',
                        type=Path,
                        required=True,
                        help='Public key filename for the "Targets" role')
    return parser.parse_args()


def main(out: Path, root_key: Path, targets_key: Path, version: int) -> None:
    """Generates and writes to disk an unsigned SignedRootMetadata."""

    root_metadata = gen_root_metadata(root_key.read_bytes(),
                                      targets_key.read_bytes(), version)
    signed = SignedRootMetadata(
        serialized_root_metadata=root_metadata.SerializeToString())
    out.write_bytes(signed.SerializeToString())


if __name__ == '__main__':
    main(**vars(parse_args()))
