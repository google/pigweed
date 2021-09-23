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
"""Generate and serialize update bundles."""

import argparse
import logging
from pathlib import Path
from typing import Dict, Iterable, Optional, Tuple

from pw_software_update import metadata
from pw_software_update.tuf_pb2 import SignedTargetsMetadata
from pw_software_update.update_bundle_pb2 import UpdateBundle

_LOG = logging.getLogger(__package__)


def gen_unsigned_update_bundle(
        tuf_repo: Path,
        exclude: Iterable[Path] = tuple(),
        remap_paths: Optional[Dict[Path, str]] = None) -> UpdateBundle:
    """Given a set of targets, generates an unsigned UpdateBundle.

    Args:
      tuf_repo: Path to a directory which will be ingested as a TUF repository.
      exclude: Iterable of paths in tuf_repo to exclude from the UpdateBundle.
      remap_paths: Dict mapping paths in tuf_repo to new target file names.

    The input directory will be treated as a TUF repository for the purposes of
    building an UpdateBundle instance. Each file in the input directory will be
    read in as a target file, unless its path (relative to the TUF repo root) is
    among the excludes.

    Default behavior is to treat TUF repo root-relative paths as the strings to
    use as targets file names, but remapping can be used to change a target file
    name to any string. If some remappings are provided but a file is found that
    does not have a remapping, a warning will be logged. If a remap is declared
    for a file that does not exist, FileNotFoundError will be raised.
    """
    if not tuf_repo.is_dir():
        raise ValueError('TUF repository must be a directory.')
    target_payloads = {}
    for path in tuf_repo.glob('**/*'):
        if path.is_dir():
            continue

        rel_path = path.relative_to(tuf_repo)
        if rel_path in exclude:
            continue

        target_file_name = str(rel_path.as_posix())
        if remap_paths:
            if rel_path in remap_paths:
                target_file_name = remap_paths[rel_path]
            else:
                _LOG.warning('Some remaps defined, but not "%s"',
                             target_file_name)
        target_payloads[target_file_name] = path.read_bytes()

    if remap_paths is not None:
        for original_path, new_target_file_name in remap_paths.items():
            if new_target_file_name not in target_payloads:
                raise FileNotFoundError(
                    f'Unable to remap "{original_path}" to'
                    f' "{new_target_file_name}"; file not found in TUF'
                    ' repository.')

    targets_metadata = metadata.gen_targets_metadata(target_payloads)
    unsigned_targets_metadata = SignedTargetsMetadata(
        serialized_targets_metadata=targets_metadata.SerializeToString())
    return UpdateBundle(
        targets_metadata=dict(targets=unsigned_targets_metadata),
        target_payloads=target_payloads)


def parse_remap_arg(remap_arg: str) -> Tuple[Path, str]:
    """Parse the string passed in to the remap argument.

    Remap strings take the following form:
      "<ORIGINAL FILENAME> > <NEW TARGET PATH>"

    For example:
      "fw_images/main_image.bin > main"
    """
    try:
        original_path, new_target_file_name = remap_arg.split('>')
        return Path(original_path.strip()), new_target_file_name.strip()
    except ValueError as err:
        raise ValueError('Path remaps must be strings of the form:\n'
                         '  "<ORIGINAL PATH> > <NEW TARGET PATH>"') from err


def parse_args():
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-t',
                        '--tuf-repo',
                        type=Path,
                        required=True,
                        help='Directory to ingest as TUF repository')
    parser.add_argument('-o',
                        '--out',
                        type=Path,
                        required=True,
                        help='Output path for serialized UpdateBundle')
    parser.add_argument('-e',
                        '--exclude',
                        type=Path,
                        nargs='+',
                        default=tuple(),
                        help='Exclude paths from the TUF repository')
    parser.add_argument('-r',
                        '--remap',
                        type=str,
                        nargs='+',
                        default=tuple(),
                        help='Remap paths to custom target file names')
    return parser.parse_args()


def main(tuf_repo: Path, out: Path, exclude: Iterable[Path],
         remap: Iterable[str]) -> None:
    """Generates an UpdateBundle and serializes it to disk."""
    remap_paths = {}
    for remap_arg in remap:
        path, new_target_file_name = parse_remap_arg(remap_arg)
        remap_paths[path] = new_target_file_name

    bundle = gen_unsigned_update_bundle(tuf_repo, exclude, remap_paths)
    out.write_bytes(bundle.SerializeToString())


if __name__ == '__main__':
    logging.basicConfig()
    main(**vars(parse_args()))
