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
"""Generates nanopb_pb2.py.

This script does two things:
* Generate nanopb_pb2.py to the specified directory.
* Ensures Nanopb supports NANOPB_PB2_NO_REBUILD.

All versions of nanopb that do not support NANOPB_PB2_NO_REBUILD (<0.4.9) have
non-deterministic behaviors when ran as part of a larger build system.
Pigweed does not support older nanopb versions that do not respect
NANOPB_PB2_NO_REBUILD.
"""

import argparse
import importlib.util
from pathlib import Path
import os
import subprocess
import sys
import warnings


def _check_bad_pb2_not_regenerated(nanopb_root: Path, nanopb_pb2_dir: Path):
    """Ensures the nanopb version supports NANOPB_PB2_NO_REBUILD."""
    # Unfortunately, nanopb looks here first. Always complain if this file
    # exits.
    default_nanopb_pb2_location = (
        nanopb_root / 'generator' / 'proto' / 'nanopb_pb2.py'
    )
    if default_nanopb_pb2_location.is_file():
        print(
            'WARNING: removing error-prone '
            f'{default_nanopb_pb2_location.resolve()}'
        )
        os.remove(default_nanopb_pb2_location)

    os.environ['NANOPB_PB2_NO_REBUILD'] = '1'

    # Add required PYTHONPATH entries.
    sys.path.insert(0, str(nanopb_pb2_dir))
    sys.path.insert(0, str(nanopb_root / 'generator'))

    spec = importlib.util.spec_from_file_location(
        'proto', nanopb_root / 'generator' / 'nanopb_generator.py'
    )
    assert spec is not None
    proto_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(proto_module)  # type: ignore[union-attr]

    assert not default_nanopb_pb2_location.is_file(), (
        'Configured nanopb does not support deterministic builds, '
        'please upgrade to 0.4.9 or newer'
    )


def generate_nanopb_proto(
    nanopb_root: Path,
    nanopb_pb2_dir: Path,
    protoc_binary: Path,
    aggressive_regen: bool,
) -> None:
    """Generate nanopb_pb2.py to nanopb_pb2_dir."""
    nanopb_proto = nanopb_root / 'generator' / 'proto' / 'nanopb.proto'

    if aggressive_regen:
        warnings.warn(
            '--aggressive-regen does nothing and will soon be removed',
            DeprecationWarning,
        )

    check_for_bad_nanopb = True
    if not nanopb_pb2_dir:
        print(
            'WARNING: Generating nanopb_pb2.py to source dir is unsupported '
            'and will be removed imminently'
        )
        nanopb_pb2_dir = nanopb_proto.parent
        check_for_bad_nanopb = False
    os.makedirs(nanopb_pb2_dir, exist_ok=True)
    subprocess.run(
        [
            protoc_binary if protoc_binary else 'protoc',
            f'--python_out={nanopb_pb2_dir}',
            nanopb_proto,
            f'-I={nanopb_proto.parent}',
        ],
        check=True,
    )
    generated_nanopb_pb2 = nanopb_pb2_dir / 'nanopb_pb2.py'
    assert generated_nanopb_pb2.is_file(), 'Could not generate nanopb_pb2.py'

    if check_for_bad_nanopb:
        _check_bad_pb2_not_regenerated(nanopb_root, nanopb_pb2_dir)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--nanopb-root',
        type=Path,
        help='Nanopb root',
    )
    parser.add_argument(
        '--nanopb-pb2-dir',
        type=Path,
        help='Directory to write the generated nanopb_pb2.py to',
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
        help='Deprecated, does nothing',
    )
    return parser.parse_args()


if __name__ == '__main__':
    generate_nanopb_proto(**vars(_parse_args()))
