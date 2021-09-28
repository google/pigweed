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
"""Metadata signing facilities."""

import argparse
from pathlib import Path

from pw_software_update import keys
from pw_software_update.tuf_pb2 import (RootMetadata, SignedRootMetadata)


def sign_root_metadata(root_metadata: SignedRootMetadata,
                       root_key_pem: bytes) -> SignedRootMetadata:
    """Signs or re-signs a Root Metadata.

    Args:
      root_metadata: A SignedRootMetadata to be signed/re-signed.
      root_key_pem: The Root signing key in PEM.
    """

    metadata = RootMetadata.FromString(root_metadata.serialized_root_metadata)

    signature = keys.create_ecdsa_signature(
        root_metadata.serialized_root_metadata, root_key_pem)

    if signature.key_id not in [k.key_id for k in metadata.keys]:
        raise ValueError('The root key is not listed in the root metadata.')

    if signature.key_id not in metadata.root_signature_requirement.key_ids:
        raise ValueError('The root key is not pinned to the root role.')

    root_metadata.signatures.append(signature)

    return root_metadata


def parse_args():
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('--root-metadata',
                        type=Path,
                        required=True,
                        help='The root metadata to be signed')

    parser.add_argument('--key',
                        type=Path,
                        required=True,
                        help='Path to the signing key')
    return parser.parse_args()


def main(root_metadata: Path, key: Path) -> None:
    """Signs/Re-signs a root metadata with a development key."""
    to_sign = SignedRootMetadata()
    to_sign.ParseFromString(root_metadata.read_bytes())
    signed = sign_root_metadata(to_sign, key.read_bytes())
    root_metadata.write_bytes(signed.SerializeToString())


if __name__ == '__main__':
    main(**vars(parse_args()))
