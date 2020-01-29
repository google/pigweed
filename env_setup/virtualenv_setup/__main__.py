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
"""Runnable module that sets up virtualenv for Pigweed."""

import argparse
import os
import sys

# TODO(mohrr) remove import-error disabling, not sure why pylint has issues
# with it.
import install  # pylint: disable=import-error


def _main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--venv_path',
                        required=True,
                        help='Path at which to create the venv')
    parser.add_argument('-r',
                        '--requirements',
                        default=[],
                        action='append',
                        help='requirements.txt files to install')
    parser.add_argument('--quick-setup',
                        dest='full_envsetup',
                        action='store_false',
                        default='PW_ENVSETUP_FULL' in os.environ,
                        help=('Do full setup or only minimal checks to see if '
                              'full setup is required.'))
    parser.add_argument('--python',
                        default=sys.executable,
                        help='Python to use when creating virtualenv.')

    try:
        install.install(**vars(parser.parse_args()))
    except install.GitRepoNotFound:
        print('git repository not found', file=sys.stderr)
        return -1

    return 0


if __name__ == '__main__':
    sys.exit(_main())
