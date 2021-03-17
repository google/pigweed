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
"""Script that invokes protoc to generate code for .proto files."""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys
import textwrap
from typing import Dict, List, Set, TextIO

try:
    from pw_build.mirror_tree import mirror_paths
except ImportError:
    # Append this path to the module search path to allow running this module
    # before the pw_build package is installed.
    sys.path.append(str(Path(__file__).resolve().parent.parent))
    from pw_build.mirror_tree import mirror_paths


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('--file-list',
                        required=True,
                        type=argparse.FileType('r'),
                        help='A list of files to copy')
    parser.add_argument('--file-list-root',
                        required=True,
                        type=argparse.FileType('r'),
                        help='A file with the root of the file list')
    parser.add_argument('--label', help='Label for this Python package')
    parser.add_argument('--proto-library',
                        default='',
                        help='Name of proto library nested in this package')
    parser.add_argument('--proto-library-file',
                        type=Path,
                        help="File with the proto library's name")
    parser.add_argument('--root',
                        required=True,
                        type=Path,
                        help='The base directory for the Python package')
    parser.add_argument('--setup-json',
                        required=True,
                        type=argparse.FileType('r'),
                        help='setup.py keywords as JSON')
    parser.add_argument('--module-as-package',
                        action='store_true',
                        help='Generate an __init__.py that imports everything')
    parser.add_argument('files',
                        type=Path,
                        nargs='+',
                        help='Relative paths to the files in the package')
    return parser.parse_args()


def _check_nested_protos(label: str, proto_library_file: Path,
                         proto_library: str) -> None:
    """Checks that the proto library refers to this package; returns error."""
    error = 'not set'

    if proto_library_file.exists():
        proto_label = proto_library_file.read_text().strip()
        if proto_label == label:
            return

        if proto_label:
            error = f'set to {proto_label}'

    raise ValueError(
        f"{label}'s 'proto_library' is set to {proto_library}, but that "
        f"target's 'python_package' is {error}. Set {proto_library}'s "
        f"'python_package' to {label}.")


def _collect_all_files(files: List[Path], root: Path, file_list: TextIO,
                       file_list_root: TextIO) -> Dict[str, Set[str]]:
    """Collects files in output dir, adds to files; returns package_data."""
    root.mkdir(exist_ok=True)

    other_files = [Path(p.rstrip()) for p in file_list]
    other_files_root = Path(file_list_root.read().rstrip())

    # Mirror the proto files to this package.
    files += mirror_paths(other_files_root, other_files, root)

    # Find all subpackages, including empty ones.
    subpackages: Set[Path] = set()
    for file in (f.relative_to(root) for f in files):
        subpackages.update(root / path for path in file.parents)
    subpackages.remove(root)

    # Make sure there are __init__.py and py.typed files for each subpackage.
    for pkg in subpackages:
        for file in (pkg / name for name in ['__init__.py', 'py.typed']):
            if not file.exists():
                file.touch()
            files.append(file)

    pkg_data: Dict[str, Set[str]] = defaultdict(set)

    # Add all non-source files to package data.
    for file in (f for f in files if f.suffix != '.py'):
        pkg = root / file.parent
        package_name = pkg.relative_to(root).as_posix().replace('/', '.')
        pkg_data[package_name].add(file.name)

    return pkg_data


_SETUP_PY_FILE = '''\
# Generated file. Do not modify.
# pylint: skip-file

import setuptools  # type: ignore

setuptools.setup(
{keywords}
)
'''


def _generate_setup_py(pkg_data: dict, setup_json: TextIO) -> str:
    setup_keywords = dict(
        packages=list(pkg_data),
        package_data={pkg: list(files)
                      for pkg, files in pkg_data.items()},
    )

    specified_keywords = json.load(setup_json)

    assert not any(kw in specified_keywords for kw in setup_keywords), (
        'Generated packages may not specify "packages" or "package_data"')
    setup_keywords.update(specified_keywords)

    return _SETUP_PY_FILE.format(keywords='\n'.join(
        f'    {k}={v!r},' for k, v in setup_keywords.items()))


def _import_module_in_package_init(all_files: List[Path]) -> None:
    """Generates an __init__.py that imports the module.

    This makes an individual module usable as a package. This is used for proto
    modules.
    """
    sources = [
        f for f in all_files if f.suffix == '.py' and f.name != '__init__.py'
    ]
    assert len(sources) == 1, (
        'Module as package expects a single .py source file')

    source, = sources
    source.parent.joinpath('__init__.py').write_text(
        f'from {source.stem}.{source.stem} import *\n')


def main(files: List[Path],
         root: Path,
         file_list: TextIO,
         file_list_root: TextIO,
         module_as_package: bool,
         setup_json: TextIO,
         label: str,
         proto_library: str = '',
         proto_library_file: Path = None) -> int:
    """Generates a setup.py and other files for a Python package."""
    if proto_library_file:
        try:
            _check_nested_protos(label, proto_library_file, proto_library)
        except ValueError as error:
            msg = '\n'.join(textwrap.wrap(str(error), 78))
            print(
                f'ERROR: Failed to generate Python package {label}:\n\n'
                f'{textwrap.indent(msg, "  ")}\n',
                file=sys.stderr)
            return 1

    pkg_data = _collect_all_files(files, root, file_list, file_list_root)

    if module_as_package:
        _import_module_in_package_init(files)

    # Create the setup.py file for this package.
    root.joinpath('setup.py').write_text(
        _generate_setup_py(pkg_data, setup_json))

    return 0


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
