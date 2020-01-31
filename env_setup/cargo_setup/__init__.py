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
"""Installs rust tools using cargo."""

import os
import subprocess


def install(pw_root, env):
    """Installs rust tools using cargo."""
    prefix = os.path.join(pw_root, '.cargo')

    # Adding to PATH at the beginning to suppress a warning about this not
    # being in PATH.
    env.prepend('PATH', os.path.join(prefix, 'bin'))

    # packages.txt contains packages one per line with two fields: package
    # name and version.
    package_path = os.path.join(pw_root, 'env_setup', 'cargo_setup',
                                'packages.txt')
    with env(), open(package_path, 'r') as ins:
        for line in ins:
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            package, version = line.split()
            cmd = [
                'cargo',
                'install',
                # If downgrading (which could happen when switching branches)
                # '--force' is required.
                '--force',
                '--root', prefix,
                '--version', version,
                package,
            ]  # yapf: disable

            print(' '.join(cmd))

            subenv = None
            if 'CARGO_TARGET_DIR' not in os.environ:
                subenv = os.environ.copy()
                subenv['CARGO_TARGET_DIR'] = os.path.expanduser(
                    '~/.cargo-cache')

            subprocess.check_call(cmd, env=subenv)
