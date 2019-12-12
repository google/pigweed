# Copyright 2019 The Pigweed Authors
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
"""Script that emits a helpful error when a facade is used without a backend."""

import argparse
import sys


def parse_args():
    """Parses command-line arguments."""

    parser = argparse.ArgumentParser(
        description='Emits an error when a facade has a null backend')
    parser.add_argument('facade_name', help='The facade with a null backend')
    return parser.parse_args()


def main():
    args = parse_args()
    print(f'ERROR: {args.facade_name} tried to build without a backend.')
    print('If you are using this module, ensure you have configured a backend '
          'properly. If you are NOT using this module, this error may have '
          'been triggered by trying to build all targets.')
    sys.exit(1)


if __name__ == '__main__':
    main()
