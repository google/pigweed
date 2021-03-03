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
"""Copies files from CIPD."""

import argparse
import json
import logging
import os
import shutil
import subprocess
import sys

logger = logging.getLogger(__name__)


def parse_args():
    parser = argparse.ArgumentParser(description='Copy file from CIPD')
    parser.add_argument('--verbose',
                        '-v',
                        help='Verbose output',
                        action='store_true')
    parser.add_argument('--manifest', help='Path to CIPD JSON file')
    parser.add_argument('--out', help='Output folder', default='.')
    parser.add_argument('--package-name',
                        required=True,
                        help='Basename of CIPD package')
    # TODO(pwbug/334) Support multiple values for --file-name.
    parser.add_argument('--file-name',
                        required=True,
                        help='Basename of file in CIPD package')
    parser.add_argument('--cipd-var',
                        required=True,
                        help='Name of CIPD directory environment variable')
    return parser.parse_args()


def check_version(manifest, cipd_path, package_name):
    instance_id_path = os.path.join(cipd_path, '.versions',
                                    f'{package_name}.cipd_version')
    with open(instance_id_path, 'r') as ins:
        instance_id = json.load(ins)['instance_id']

    with open(manifest, 'r') as ins:
        data = json.load(ins)

    path = None
    expected_version = None
    for entry in data:
        if package_name in entry['path']:
            path = entry['path']
            expected_version = entry['tags'][0]
    if not path:
        raise LookupError(f'failed to find {package_name} entry')

    cmd = ['cipd', 'describe', path, '-version', instance_id]
    output = subprocess.check_output(cmd).decode()
    if expected_version not in output:
        # TODO(pwbug/334) Update package if it's out of date.
        raise ValueError(f'{package_name} is out of date')


def main():
    args = parse_args()

    if args.verbose:
        logger.setLevel(logging.DEBUG)

    try:
        cipd_path = os.environ[args.cipd_var]
    except KeyError:
        logger.error(
            "The %s environment variable isn't set. Did you forget to run "
            '`. ./bootstrap.sh`?', args.cipd_var)
        sys.exit(1)

    check_version(args.manifest, cipd_path, args.package_name)

    shutil.copyfile(os.path.join(cipd_path, args.file_name),
                    os.path.join(args.out, args.file_name))


if __name__ == '__main__':
    logging.basicConfig()
    main()
