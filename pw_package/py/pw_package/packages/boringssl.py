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
"""Install and check status of BoringSSL + Chromium verifier."""

import os
import pathlib
import subprocess
from typing import Sequence
import pw_package.git_repo
import pw_package.package_manager


def boringssl_src_path(path: pathlib.Path):
    return path / 'src'


class BoringSSLLibrary(pw_package.git_repo.GitRepo):
    """Install and check status of BoringSSL library part"""
    def status(self, path: pathlib.Path) -> bool:
        if not super().status(boringssl_src_path(path)):
            return False

        # Check that necessary build files are generated.
        build_files = ['BUILD.generated.gni', 'err_data.c']
        return all([os.path.exists(path / file) for file in build_files])

    def install(self, path: pathlib.Path) -> None:
        if self.status(path):
            return

        src_path = boringssl_src_path(path)
        # Checkout the library
        super().install(src_path)
        # BoringSSL provides a src/util/generate_build_files.py script for
        # generating build files. Call the script after checkout so that
        # our .gn build script can pick them up.
        script = src_path / 'util' / 'generate_build_files.py'
        if not os.path.exists(script):
            raise FileNotFoundError('Fail to find generate_build_files.py')
        subprocess.run(['python', script, 'gn'], cwd=path)


class BoringSSL(pw_package.package_manager.Package):
    """Install and check status of BoringSSL and chromium verifier."""
    def __init__(self, *args, **kwargs):
        super().__init__(*args, name='boringssl', **kwargs)
        self._boringssl = BoringSSLLibrary(
            name='boringssl',
            url=''.join([
                'https://pigweed.googlesource.com',
                '/third_party/boringssl/boringssl'
            ]),
            commit='9f55d972854d0b34dae39c7cd3679d6ada3dfd5b')

    def boringssl_path(self, path: pathlib.Path) -> pathlib.Path:
        return path / self._boringssl.name

    def status(self, path: pathlib.Path) -> bool:
        if not self._boringssl.status(self.boringssl_path(path)):
            return False

        # TODO(zyecheng): Add status logic for chromium certificate verifier.
        return True

    def install(self, path: pathlib.Path) -> None:
        self._boringssl.install(self.boringssl_path(path))

        # TODO(zyecheng): Add install logic for chromium certificate verifier.

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (
            f'{self.name} installed in: {path}',
            'Enable by running "gn args out" and adding this line:',
            f'  dir_pw_third_party_boringssl = "{self.boringssl_path(path)}"',
            # TODO(zyecheng): Add variable instruction for chromium
            # certificate verifier
        )


pw_package.package_manager.register(BoringSSL)
