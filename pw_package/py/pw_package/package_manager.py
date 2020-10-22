# Copyright 2020 The Pigweed Authors
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
"""Install and remove optional packages."""

import argparse
import logging
import os
import pathlib
import shutil
from typing import List

_LOG: logging.Logger = logging.getLogger(__name__)


class Package:
    """Package to be installed.

    Subclass this to implement installation of a specific package.
    """
    def __init__(self, name):
        self._name = name

    @property
    def name(self):
        return self._name

    def install(self, path: pathlib.Path) -> None:  # pylint: disable=no-self-use
        """Install the package at path.

        Install the package in path. Cannot assume this directory is empty—it
        may need to be deleted or updated.
        """

    def remove(self, path: pathlib.Path) -> None:  # pylint: disable=no-self-use
        """Remove the package from path.

        Removes the directory containing the package. For most packages this
        should be sufficient to remove the package, and subclasses should not
        need to override this package.
        """
        if os.path.exists(path):
            shutil.rmtree(path)

    def status(self, path: pathlib.Path) -> bool:  # pylint: disable=no-self-use
        """Returns if package is installed at path and current.

        This method will be skipped if the directory does not exist.
        """


_PACKAGES = {}


def register(package_class: type) -> None:
    obj = package_class()
    _PACKAGES[obj.name] = obj


class PackageManager:
    """Install and remove optional packages."""
    def __init__(self):
        self._pkg_root: pathlib.Path = None

    def install(self, package: str, force=False):
        pkg = _PACKAGES[package]
        if force:
            self.remove(package)
        _LOG.info('Installing %s...', pkg.name)
        pkg.install(self._pkg_root / pkg.name)
        _LOG.info('Installing %s...done.', pkg.name)
        return 0

    def remove(self, package: str):  # pylint: disable=no-self-use
        pkg = _PACKAGES[package]
        _LOG.info('Removing %s...', pkg.name)
        pkg.remove(self._pkg_root / pkg.name)
        _LOG.info('Removing %s...done.', pkg.name)
        return 0

    def status(self, package: str):  # pylint: disable=no-self-use
        pkg = _PACKAGES[package]
        path = self._pkg_root / pkg.name
        if os.path.isdir(path) and pkg.status(path):
            _LOG.info('%s is installed.', pkg.name)
            return 0

        _LOG.info('%s is not installed.', pkg.name)
        return -1

    def list(self):  # pylint: disable=no-self-use
        _LOG.info('Installed packages:')
        available = []
        for package in sorted(_PACKAGES.keys()):
            pkg = _PACKAGES[package]
            if pkg.status(self._pkg_root / pkg.name):
                _LOG.info('  %s', pkg.name)
            else:
                available.append(pkg.name)
        _LOG.info('')

        _LOG.info('Available packages:')
        for pkg_name in available:
            _LOG.info('  %s', pkg_name)
        _LOG.info('')

        return 0

    def run(self, command: str, pkg_root: pathlib.Path, **kwargs):
        os.makedirs(pkg_root, exist_ok=True)
        self._pkg_root = pkg_root
        return getattr(self, command)(**kwargs)


def parse_args(argv: List[str] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser("Manage packages.")
    parser.add_argument(
        '--package-root',
        '-e',
        dest='pkg_root',
        type=pathlib.Path,
        default=(pathlib.Path(os.environ['_PW_ACTUAL_ENVIRONMENT_ROOT']) /
                 'packages'),
    )
    subparsers = parser.add_subparsers(dest='command', required=True)
    install = subparsers.add_parser('install')
    install.add_argument('--force', '-f', action='store_true')
    remove = subparsers.add_parser('remove')
    status = subparsers.add_parser('status')
    for cmd in (install, remove, status):
        cmd.add_argument('package', choices=_PACKAGES.keys())
    _ = subparsers.add_parser('list')
    return parser.parse_args(argv)


def run(**kwargs):
    return PackageManager().run(**kwargs)
