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
"""Build a Python Source tree."""

import argparse
import configparser
from datetime import datetime
import io
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from typing import Iterable

from pw_build.python_package import PythonPackage, load_packages


def _parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree-destination-dir',
                        type=Path,
                        help='Path to output directory.')
    parser.add_argument('--include-tests',
                        action='store_true',
                        help='Include tests in the tests dir.')

    parser.add_argument('--setupcfg-common-file',
                        type=Path,
                        help='A file containing the common set of options for'
                        'incluing in the merged setup.cfg provided version.')
    parser.add_argument('--setupcfg-version-append-git-sha',
                        action='store_true',
                        help='Append the current git SHA to the setup.cfg '
                        'version.')
    parser.add_argument('--setupcfg-version-append-date',
                        action='store_true',
                        help='Append the current date to the setup.cfg '
                        'version.')

    parser.add_argument(
        '--extra-files',
        nargs='+',
        help='Paths to extra files that should be included in the output dir.')

    parser.add_argument(
        '--input-list-files',
        nargs='+',
        type=Path,
        help='Paths to text files containing lists of Python package metadata '
        'json files.')

    return parser.parse_args()


class UnknownGitSha(Exception):
    "Exception thrown when the current git SHA cannot be found."


def get_current_git_sha() -> str:
    git_command = 'git log -1 --pretty=format:%h'
    process = subprocess.run(git_command.split(),
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    gitsha = process.stdout.decode()
    if process.returncode != 0 or not gitsha:
        error_output = f'\n"{git_command}" failed with:' f'\n{gitsha}'
        if process.stderr:
            error_output += f'\n{process.stderr.decode()}'
        raise UnknownGitSha('Could not determine the current git SHA.' +
                            error_output)
    return gitsha.strip()


def get_current_date() -> str:
    return datetime.now().strftime('%Y%m%d%H%M')


class UnexpectedConfigSection(Exception):
    "Exception thrown when the common configparser contains unexpected values."


def load_common_config(common_config: Path,
                       append_git_sha: bool = False,
                       append_date: bool = False) -> configparser.ConfigParser:
    """Load an existing ConfigParser file and update metadata.version."""
    config = configparser.ConfigParser()
    config.read(common_config)

    # Check for existing values that should not be present
    if config.has_option('options', 'packages'):
        value = str(config['options']['packages'])
        raise UnexpectedConfigSection(
            f'[options] packages already defined as: {value}')

    if config.has_section('options.package_data'):
        raise UnexpectedConfigSection(
            '[options.package_data] already defined as:\n' +
            str(dict(config['options.package_data'].items())))

    if config.has_section('options.entry_points'):
        raise UnexpectedConfigSection(
            '[options.entry_points] already defined as:\n' +
            str(dict(config['options.entry_points'].items())))

    # Metadata and option sections should already be defined.
    assert config.has_section('metadata')
    assert config.has_section('options')

    # Append build metadata if applicable.
    build_metadata = []
    if append_date:
        build_metadata.append(get_current_date())
    if append_git_sha:
        build_metadata.append(get_current_git_sha())
    if build_metadata:
        version_prefix = config['metadata']['version']
        build_metadata_text = '.'.join(build_metadata)
        config['metadata']['version'] = (
            f'{version_prefix}+{build_metadata_text}')
    return config


def update_config_with_packages(
    config: configparser.ConfigParser,
    python_packages: Iterable[PythonPackage],
) -> None:
    """Merge setup.cfg files from a set of python packages."""
    config['options']['packages'] = 'find:'
    config['options.package_data'] = {}
    config['options.entry_points'] = {}

    # Save a list of packages being bundled.
    included_packages = [pkg.package_name for pkg in python_packages]

    for pkg in python_packages:
        assert pkg.config

        # Collect install_requires
        if pkg.config.has_option('options', 'install_requires'):
            existing_requires = config['options'].get('install_requires', '\n')
            # Requires are delimited by newlines or semicolons.
            # Split existing list on either one.
            this_requires = re.split(r' *[\n;] *',
                                     pkg.config['options']['install_requires'])
            new_requires = existing_requires.splitlines() + this_requires
            # Remove requires already included in this merged config.
            new_requires = [
                line for line in new_requires
                if line and line not in included_packages
            ]
            # Remove duplictes and sort require list.
            new_requires_text = '\n' + '\n'.join(sorted(set(new_requires)))
            config['options']['install_requires'] = new_requires_text

        # Collect package_data
        if pkg.config.has_section('options.package_data'):
            for key, value in pkg.config['options.package_data'].items():
                config['options.package_data'][key] = value

        # Collect entry_points
        if pkg.config.has_section('options.entry_points'):
            for key, value in pkg.config['options.entry_points'].items():
                existing_entry_points = config['options.entry_points'].get(
                    key, '')
                new_entry_points = '\n'.join([existing_entry_points, value])
                # Remove any empty lines
                new_entry_points = new_entry_points.replace('\n\n', '\n')
                config['options.entry_points'][key] = new_entry_points


def write_config(
    common_config: Path,
    final_config: configparser.ConfigParser,
    tree_destination_dir: Path,
) -> None:
    """Write a the final setup.cfg file with license comment block."""
    # Get the license comment block from the common_config.
    comment_block_text = ''
    comment_block_match = re.search(r'((^#.*?[\r\n])*)([^#])',
                                    common_config.read_text(), re.MULTILINE)
    if comment_block_match:
        comment_block_text = comment_block_match.group(1)

    setup_cfg_file = tree_destination_dir.resolve() / 'setup.cfg'
    setup_cfg_text = io.StringIO()
    final_config.write(setup_cfg_text)
    setup_cfg_file.write_text(comment_block_text + setup_cfg_text.getvalue())


def build_python_tree(python_packages: Iterable[PythonPackage],
                      tree_destination_dir: Path,
                      include_tests: bool = False) -> None:
    """Install PythonPackages to a destination directory."""

    # Create the root destination directory.
    destination_path = tree_destination_dir.resolve()
    # Delete any existing files
    shutil.rmtree(destination_path, ignore_errors=True)
    destination_path.mkdir(exist_ok=True)

    # Define a temporary location to run setup.py build in.
    with tempfile.TemporaryDirectory() as build_base_name:
        build_base = Path(build_base_name)

        for pkg in python_packages:
            lib_dir_path = pkg.setuptools_build_with_base(
                build_base, include_tests=include_tests)

            # Move installed files from the temp build-base into
            # destination_path.
            for new_file in lib_dir_path.glob('*'):
                # Use str(Path) since shutil.move only accepts path-like objects
                # in Python 3.9 and up:
                #   https://docs.python.org/3/library/shutil.html#shutil.move
                shutil.move(str(new_file), str(destination_path))

            # Clean build base lib folder for next install
            shutil.rmtree(lib_dir_path, ignore_errors=True)


def copy_extra_files(extra_file_strings: Iterable[str]) -> None:
    """Copy extra files to their destinations."""
    if not extra_file_strings:
        return

    for extra_file_string in extra_file_strings:
        # Convert 'source > destination' strings to Paths.
        input_output = re.split(r' *> *', extra_file_string)
        source_file = Path(input_output[0])
        dest_file = Path(input_output[1])

        if not source_file.exists():
            raise FileNotFoundError(f'extra_file "{source_file}" not found.\n'
                                    f'  Defined by: "{extra_file_string}"')

        # Copy files and make parent directories.
        dest_file.parent.mkdir(parents=True, exist_ok=True)
        # Raise an error if the destination file already exists.
        if dest_file.exists():
            raise FileExistsError(
                f'Copying "{source_file}" would overwrite "{dest_file}"')

        shutil.copy(source_file, dest_file)


def main():
    args = _parse_args()

    py_packages = load_packages(args.input_list_files)

    build_python_tree(python_packages=py_packages,
                      tree_destination_dir=args.tree_destination_dir,
                      include_tests=args.include_tests)
    copy_extra_files(args.extra_files)

    if args.setupcfg_common_file:
        config = load_common_config(
            common_config=args.setupcfg_common_file,
            append_git_sha=args.setupcfg_version_append_git_sha,
            append_date=args.setupcfg_version_append_date)

        update_config_with_packages(config=config, python_packages=py_packages)

        write_config(common_config=args.setupcfg_common_file,
                     final_config=config,
                     tree_destination_dir=args.tree_destination_dir)


if __name__ == '__main__':
    main()
