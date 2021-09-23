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
"""Facilities for generating signing keys for development use."""

import argparse
import logging
from pathlib import Path
from typing import Optional

from securesystemslib.interface import (  # type: ignore
    generate_and_write_unencrypted_ecdsa_keypair)

_LOG = logging.getLogger(__package__)


def parse_args():
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-o',
                        '--out',
                        type=Path,
                        default=None,
                        help='Output path for the generated key')
    return parser.parse_args()


def gen_ecdsa_sha2_nistp256_keypair(out: Optional[Path] = None) -> Path:
    """Generates ecdsa key pair and writes custom JSON-formatted keys to disk.

    Args:
      out(optional): The path to write the private key to. If not passed,
        the key is written to CWD using the keyid as filename. The public key
        is written to the same path as the private key using the suffix '.pub'.

    NOTE: The custom key format includes 'ecdsa-sha2-nistp256' as signing
      scheme.

    WARNING: The generated keys are FOR DEVELOPMENT USE ONLY. They SHALL NEVER
      be used for production signing.

    Side Effects:
      Writes key files to disk.

    Returns:
      The private key filepath.
    """
    priviate_key_name = generate_and_write_unencrypted_ecdsa_keypair(
        str(out) if out else None)

    _LOG.warning('DEVELOPMENT-USE-ONLY private key written to: "%s"',
                 priviate_key_name)
    _LOG.warning('DEVELOPMENT-USE-ONLY public key written to: "%s.pub"',
                 priviate_key_name)

    return Path(priviate_key_name)


def main(out: Optional[Path] = None) -> None:
    """Generates and writes to disk key pairs for development use.

    Args:
      out(optional): The path to write the private key to. If not passed,
        the key is written to CWD using the keyid as filename. The public key
        is written to the same path as the private key using the suffix '.pub'.
    """

    # Currently only supports the "ecdsa-sha2-nistp256" key scheme.
    #
    # TODO(alizhang): Add support for "rsassa-pss-sha256" and "ed25519" key
    # schemes.
    gen_ecdsa_sha2_nistp256_keypair(out)


if __name__ == '__main__':
    logging.basicConfig()
    main(**vars(parse_args()))
