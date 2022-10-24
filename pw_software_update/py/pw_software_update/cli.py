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
"""
Software update related operations.

Learn more at: pigweed.dev/pw_software_update

"""

import argparse
import sys
from pathlib import Path


def generate_key_handler(arg) -> None:
    # TODO(b/254524996): Currently only prints path,
    # eventually will handle key generation
    print(arg.pathname)


def _create_generate_key_parser(subparsers) -> None:
    """Parser to handle key generation subcommand."""

    generate_key_parser = subparsers.add_parser(
        'generate-key',
        description=
        'Generates an ecdsa-sha2-nistp256 signing key pair (private + public)',
        help='')
    generate_key_parser.set_defaults(func=generate_key_handler)
    generate_key_parser.add_argument('pathname',
                                     type=Path,
                                     help='Path to generated key pair')


def _build_argument_parser() -> None:
    parser_root = argparse.ArgumentParser(
        description='Software update related operations.',
        epilog='Learn more at: pigweed.dev/pw_software_update')
    parser_root.set_defaults(
        func=lambda *_args, **_kwargs: parser_root.print_help())

    subparsers = parser_root.add_subparsers()
    _create_generate_key_parser(subparsers)

    args = parser_root.parse_args()
    args.func(args)


def main() -> int:
    """Software update command-line interface(WIP)."""
    _build_argument_parser()
    return 0


if __name__ == '__main__':
    sys.exit(main())
