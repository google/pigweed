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
"""Emits an error when using a protobuf library that is not generated"""

import argparse
import sys


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library',
                        required=True,
                        help='The protobuf library being built')
    parser.add_argument('--generator',
                        required=True,
                        help='The protobuf generator requested')
    return parser.parse_args()


def main(library: str, generator: str):
    print(f'ERROR: Attempting to build protobuf library {library}, but the '
          f'{generator} protobuf generator is not in use.')
    print(f'To use {generator} protobufs, list "{generator}" in '
          'pw_protobuf_generators.')
    sys.exit(1)


if __name__ == '__main__':
    main(**vars(parse_args()))
