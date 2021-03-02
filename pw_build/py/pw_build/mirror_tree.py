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
"""Mirrors a directory tree to another directory using hard links."""

import argparse
import os
from pathlib import Path
from typing import Iterable, List


def _parse_args() -> argparse.Namespace:
    """Registers the script's arguments on an argument parser."""

    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('--source-root',
                        type=Path,
                        required=True,
                        help='Prefix to strip from the source files')
    parser.add_argument('sources',
                        type=Path,
                        nargs='+',
                        help='Files to mirror to the directory')
    parser.add_argument('--directory',
                        type=Path,
                        required=True,
                        help='Directory to which to mirror the sources')

    return parser.parse_args()


def mirror_paths(source_root: Path, sources: Iterable[Path],
                 directory: Path) -> List[Path]:
    outputs: List[Path] = []

    for source in sources:
        dest = directory / source.relative_to(source_root)
        dest.parent.mkdir(parents=True, exist_ok=True)

        if dest.exists():
            dest.unlink()

        # Use a hard link to avoid unnecessary copies.
        os.link(source, dest)
        outputs.append(dest)

    return outputs


if __name__ == '__main__':
    mirror_paths(**vars(_parse_args()))
