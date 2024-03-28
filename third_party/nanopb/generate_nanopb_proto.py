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
"""Generates nanopb_pb2.py by importing the Nanopb proto module.

The Nanopb repository generates nanopb_pb2.py dynamically when its Python
package is imported if it does not exist. If multiple processes try to use
Nanopb to compile simultaneously on a clean build, they can interfere with each
other. One process might rewrite nanopb_pb2.py as another process is trying to
access it, resulting in import errors.

This script imports the Nanopb module so that nanopb_pb2.py is generated if it
doesn't exist. All Nanopb proto compilation targets depend on this script so
that nanopb_pb2.py is guaranteed to exist before they need it.
"""

import argparse
import importlib.util
from pathlib import Path
import os
import sys


def generate_nanopb_proto(
    nanopb_root: Path, protoc_binary: Path, aggressive_regen: bool
) -> None:
    generated_nanopb_pb2 = nanopb_root / 'generator' / 'proto' / 'nanopb_pb2.py'

    # If protoc was updated, ensure the file is regenerated.
    if generated_nanopb_pb2.is_file():
        if (
            aggressive_regen
            or protoc_binary.stat().st_mtime
            > generated_nanopb_pb2.stat().st_mtime
        ):
            os.remove(generated_nanopb_pb2)

    sys.path.append(str(nanopb_root / 'generator'))

    spec = importlib.util.spec_from_file_location(
        'proto', nanopb_root / 'generator' / 'nanopb_generator.py'
    )
    assert spec is not None
    proto_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(proto_module)  # type: ignore[union-attr]
    assert generated_nanopb_pb2.is_file(), 'Could not generate nanopb_pb2.py'


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--nanopb-root',
        type=Path,
        help='Nanopb root',
    )
    parser.add_argument(
        '--protoc-binary',
        type=Path,
        help='Protoc binary path',
    )
    parser.add_argument(
        '--aggressive-regen',
        action="store_true",
        default=False,
        help=(
            'If true, always regenerates nanopb_pb2.py when this script is '
            'run. If this is false, the file is only regenerated when the '
            'protoc binary is updated'
        ),
    )
    return parser.parse_args()


if __name__ == '__main__':
    generate_nanopb_proto(**vars(_parse_args()))
