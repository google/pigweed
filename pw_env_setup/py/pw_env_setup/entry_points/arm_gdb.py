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
"""Wrapper script to run arm-none-eabi-gdb with Python 3.8.

This is a relatively bespoke wrapper that operates on two key assumptions:
1. pw_env_setup has placed a compatible Python distribution in a `python`
   directory adjacent to the `bin` directory of the active ARM GCC toolchain.
2. The current version of arm-none-eabi-gdb requires Python 3.8.
"""

import os
from pathlib import Path
import shutil
import signal
import sys
import subprocess


def main() -> int:
    """arm-gdb wrapper that sets up the Python environment for gdb"""

    # Find 'arm-none-eabi-gcc', as Pigweed places a compatible distribution of
    # Python in the ARM GCC toolchain install base. DO NOT look for
    # `arm-none-eabi-gdb` as it can cause flakes since this script is aliased to
    # the same name (see https://pwbug.dev/326666018).
    arm_gcc_binary = shutil.which('arm-none-eabi-gcc')
    assert arm_gcc_binary, (
        'Failed to find arm-none-eabi-gcc on the PATH, perhaps it is missing '
        'from your CIPD package install list?'
    )

    arm_gdb_path = Path(arm_gcc_binary).parent / 'arm-none-eabi-gdb'
    assert arm_gdb_path.is_file(), (
        'arm-none-eabi-gdb appears to be missing from the located ARM '
        f'toolchain distribution at {arm_gdb_path.parent}'
    )

    arm_install_prefix = arm_gdb_path.parent.parent
    python_home = arm_install_prefix / 'python' / 'bin' / 'python3.8'
    python_path = arm_install_prefix / 'python' / 'lib' / 'python3.8'

    env = os.environ.copy()
    # Only set Python if it's in the expected location.
    if python_home.is_file() and python_path.is_dir():
        env['PYTHONHOME'] = str(python_home)
        env['PYTHONPATH'] = str(python_path)

    # Ignore Ctrl-C to allow gdb to handle normally
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    return subprocess.run(
        [str(arm_gdb_path)] + sys.argv[1:], env=env, check=False
    ).returncode


if __name__ == '__main__':
    sys.exit(main())
