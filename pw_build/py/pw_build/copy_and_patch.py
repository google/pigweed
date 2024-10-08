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
"""Utilities to apply a patch to a file.

The patching is done entirely in python and doesn't shell out any other tools.
"""

import argparse
import logging
from pathlib import Path
import os
import sys
import shutil
import tempfile

import patch  # type: ignore

logging.basicConfig(stream=sys.stdout, level=logging.WARN)
_LOG = logging.getLogger(__name__)


def _parse_args() -> argparse.Namespace:
    """Registers the script's arguments on an argument parser."""

    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '--patch-file',
        type=Path,
        required=True,
        help='Location of the patch file to apply.',
    )
    parser.add_argument(
        '--src',
        type=Path,
        required=True,
        help='Location of the source file to be patched.',
    )
    parser.add_argument(
        '--dst',
        type=Path,
        required=True,
        help='Destination to copy the --src file to.',
    )
    parser.add_argument(
        '--root',
        type=Path,
        default=None,
        help='Root directory for applying the patch.',
    )

    return parser.parse_args()


def _longest_common_suffix(str1: str, str2: str) -> str:
    if not str1 or not str2:
        return ""

    i = len(str1) - 1
    j = len(str2) - 1
    suffix = ""
    while i >= 0 and j >= 0 and str1[i] == str2[j]:
        suffix = str1[i] + suffix
        i -= 1
        j -= 1

    return suffix


def _get_temp_path_for_src(src: Path, root: Path) -> str:
    # Calculate the temp directory structure which matches the path of
    # the src.
    # Note in bazel the paths passed are all children of the sandbox,
    # but in GN, the paths are peers of the build directory, ie "../"
    # To handle these differences, first expand all paths to absolute
    # paths, and then make the src relative to the root.

    absolute_src = src.absolute()
    _LOG.debug("absolute_src = %s", absolute_src)
    absolute_root = Path.cwd()
    if root:
        absolute_root = root.absolute()
    _LOG.debug("absolute_root = %s", absolute_root)

    relative_src = absolute_src.relative_to(absolute_root)
    _LOG.debug("relative_src = %s", relative_src)

    return os.path.dirname(relative_src)


def copy_and_apply_patch(
    patch_file: Path, src: Path, dst: Path, root: Path
) -> int:
    """Copy then apply the diff"""

    # create a temp directory which contains the entire
    # path tree of the file to ensure the diff applies.
    tmp_dir = tempfile.TemporaryDirectory()
    full_tmp_dir = Path(tmp_dir.name, _get_temp_path_for_src(src, root))
    _LOG.debug("full_tmp_dir = %s", full_tmp_dir)
    os.makedirs(full_tmp_dir)

    tmp_src = Path(full_tmp_dir, src.name)
    shutil.copyfile(src, tmp_src, follow_symlinks=False)

    p = patch.fromfile(patch_file)
    if not p.apply(root=tmp_dir.name):
        return 1

    shutil.copyfile(tmp_src, dst, follow_symlinks=False)
    return 0


if __name__ == '__main__':
    sys.exit(copy_and_apply_patch(**vars(_parse_args())))
