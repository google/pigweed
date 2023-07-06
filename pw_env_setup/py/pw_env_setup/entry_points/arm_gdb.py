# Copyright 2023 The Pigweed Authors
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
"""Wrapper script to run arm-none-eabi-gdb with Python 3.8."""

import os
from pathlib import Path
import shutil
import signal
import sys
import subprocess


def main() -> None:
    # Find 'arm-none-eabi-gdb' as long as it isn't in the current Python
    # virtualenv entry point. In other words: not this script.
    arm_gdb_binary = shutil.which(
        'arm-none-eabi-gdb',
        path=os.pathsep.join(
            [
                path_entry
                for path_entry in os.environ.get('PATH', '').split(os.pathsep)
                if path_entry not in sys.path
            ]
        ),
    )
    assert arm_gdb_binary

    arm_gdb_path = Path(arm_gdb_binary)
    arm_install_prefix = arm_gdb_path.parent.parent
    python_home = arm_install_prefix / 'python/bin/python3'
    python_path = arm_install_prefix / 'python/lib/python3.8'
    assert arm_gdb_path.is_file()

    env = os.environ.copy()
    # Only set Python if it's in the expected location.
    if python_home.is_file() and python_path.is_dir():
        env['PYTHONHOME'] = str(python_home)
        env['PYTHONPATH'] = str(python_path)

    # Ignore Ctrl-C to allow gdb to handle normally
    signal.signal(signal.SIGINT, lambda sig, frame: None)
    subprocess.run([str(arm_gdb_path)] + sys.argv[1:], env=env, check=False)


if __name__ == '__main__':
    main()
