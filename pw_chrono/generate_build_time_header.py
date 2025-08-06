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
"""Generate a header file for UTC time"""

import argparse
from datetime import datetime
import sys

HEADER = """// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include <stdint.h>

"""


def parse_args() -> argparse.Namespace:
    """Setup argparse."""
    parser = argparse.ArgumentParser()
    parser.add_argument("out", help="path for output header file")
    parser.add_argument(
        '--bazel',
        action='store_true',
        help='Set if this script is being invoked by Bazel',
    )
    return parser.parse_args()


def main() -> int:
    """Main function"""
    args = parse_args()

    if args.bazel:
        # See https://bazel.build/docs/user-manual#workspace-status for
        # documentation of volatite-status.txt.
        with open('bazel-out/volatile-status.txt', 'r') as status:
            for line in status:
                pieces = line.split()
                key = pieces[0]
                if key == "BUILD_TIMESTAMP":
                    time_stamp = int(pieces[1])
                    break
    else:
        # We're not being invoked from Bazel.
        time_stamp = int(datetime.now().timestamp())

    with open(args.out, "w") as header:
        header.write(HEADER)

        # Add a comment in the generated header to show readable build time
        string_date = datetime.fromtimestamp(time_stamp).strftime(
            "%m/%d/%Y %H:%M:%S"
        )
        header.write(f'// {string_date}\n')

        # Write to the header.
        header.write(
            ''.join(
                [
                    'constexpr uint64_t kBuildTimeMicrosecondsUTC = ',
                    f'{int(time_stamp * 1e6)};\n',
                ]
            )
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
