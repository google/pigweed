#!/usr/bin/env python
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
"""
Zephyr CLI

The 'zephyr' subcommand for the Pigweed CLI provides functionality for
integrating a Pigweed and Zephyr application.
"""

import argparse
import os
import re
import subprocess
from pathlib import Path
import logging
import yaml

from pw_env_setup_zephyr.argparser import add_parser_arguments


_LOG = logging.getLogger('pw_zephyr.zephyr')


def get_parser() -> argparse.ArgumentParser:
    """Get a parser associated with this command."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser = add_parser_arguments(parser)
    return parser


def maybe_append_remote(remotes: list, name: str, path: Path) -> bool:
    """Try appending a remote entry if it exists and is valid."""
    if not path.exists():
        _LOG.debug("'%s' does not exist, can't add '%s'", str(path), name)
        return False
    if not path.is_dir():
        _LOG.debug("'%s' is not a directory, can't add '%s'", str(path), name)
        return False

    result = subprocess.run(
        ['git', 'remote', '-v'],
        stdout=subprocess.PIPE,
        cwd=path,
    )
    if result.returncode != 0:
        _LOG.error("Failed to find remotes for '%s' in '%s'", name, str(path))
        return False
    output = result.stdout.decode('utf-8')
    _LOG.debug("git remotes=\n%s", output)
    url_match = re.match(
        r'^\w+\s+([\w@\:\/\-\.]+)\s+\(fetch\).*$', output, re.MULTILINE
    )
    if not url_match:
        _LOG.error(
            "Failed to find (fetch) remote for '%s' in '%s'", name, str(path)
        )
        return False
    remotes.append(
        {
            'name': name,
            'url-base': url_match.group(1),
        }
    )
    return True


def append_project(projects: list, name: str, path: Path) -> None:
    """Append a project entry to the list"""
    result = subprocess.run(
        ['git', 'log', '--pretty=format:"%H"', '-n', '1'],
        stdout=subprocess.PIPE,
        cwd=path,
    )
    if result.returncode != 0:
        _LOG.error(
            "Failed to find hash branch for '%s' in '%s'", name, str(path)
        )
        return
    output: str = result.stdout.decode('utf-8').strip().strip('"')
    _LOG.debug("git current hash is '%s'", output)
    if output == '':
        _LOG.error("'%s' has no commit hash", str(path))
        return

    projects.append(
        {
            'name': name,
            'remote': name,
            'revision': output,
            'path': str(
                path.relative_to(os.path.commonpath([path, os.getcwd()]))
            ),
            'import': name == 'zephyr',
        }
    )


def generate_manifest() -> dict:
    """Generate a dictionary that matches a West manifest yaml format."""
    pw_root_dir = Path(os.environ['PW_ROOT'])
    zephyr_root_dir = pw_root_dir / 'environment' / 'packages' / 'zephyr'
    remotes: list = []
    projects: list = []
    if maybe_append_remote(remotes, 'pigweed', pw_root_dir):
        append_project(projects, 'pigweed', pw_root_dir)
    if maybe_append_remote(remotes, 'zephyr', zephyr_root_dir):
        append_project(projects, 'zephyr', zephyr_root_dir)

    return {
        'manifest': {
            'remotes': remotes,
            'projects': projects,
        },
    }


def print_manifest() -> None:
    """Print the content of a West manifest for the current Pigweed checkout"""
    _LOG.info(yaml.safe_dump(generate_manifest()))


def main() -> None:
    """Main entry point for the 'pw zephyr' command."""
    parser = get_parser()
    args = parser.parse_args()
    _LOG.addHandler(logging.StreamHandler())
    _LOG.propagate = False
    if args.verbose:
        _LOG.setLevel(logging.DEBUG)
    else:
        _LOG.setLevel(logging.INFO)

    if args.zephyr_subcommand == 'manifest':
        print_manifest()
    else:
        parser.print_usage()


if __name__ == '__main__':
    main()
