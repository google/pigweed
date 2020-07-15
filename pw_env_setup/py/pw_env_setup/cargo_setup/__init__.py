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
import sys
import tempfile


def install(install_dir, package_files, env):
    """Installs rust tools using cargo."""
    # Adding to PATH at the beginning to suppress a warning about this not
    # being in PATH.
    env.prepend('PATH', os.path.join(install_dir, 'bin'))

    if 'CARGO_TARGET_DIR' not in os.environ:
        env.set('CARGO_TARGET_DIR', os.path.expanduser('~/.cargo-cache'))

    with env():
        for package_file in package_files:
            with open(package_file, 'r') as ins:
                for line in ins:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue

                    package, version = line.split()
                    cmd = [
                        'cargo',
                        'install',
                        # If downgrading (which could happen when switching
                        # branches) '--force' is required.
                        '--force',
                        '--root', install_dir,
                        '--version', version,
                        package,
                    ]  # yapf: disable

                    # TODO(pwbug/135) Use function from common utility module.
                    with tempfile.TemporaryFile(mode='w+') as temp:
                        try:
                            subprocess.check_call(cmd,
                                                  stdout=temp,
                                                  stderr=subprocess.STDOUT)
                        except subprocess.CalledProcessError:
                            temp.seek(0)
                            sys.stderr.write(temp.read())
                            raise

    return True
