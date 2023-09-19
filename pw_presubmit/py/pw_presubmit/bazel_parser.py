# Copyright 2022 The Pigweed Authors
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
"""Extracts a concise error from a bazel log."""

from pathlib import Path
import re
import sys
from typing import List


def parse_bazel_stdout(bazel_stdout: Path) -> str:
    """Extracts a concise error from a bazel log."""
    seen_error: bool = False
    error_lines: List[str] = []

    with bazel_stdout.open() as ins:
        for line in ins:
            # Trailing whitespace isn't significant, as it doesn't affect the
            # way the line shows up in the logs. However, leading whitespace may
            # be significant, especially for compiler error messages.
            line = line.rstrip()

            if re.match(r'^(ERROR|FAIL):', line):
                seen_error = True

            if seen_error:
                # Ignore long lines that just show the environment.
                if re.search(r' +[\w_]*(PATH|PWD)=.* \\', line):
                    continue

                # Ignore lines that only show bazel sandboxing.
                if line.strip().startswith(('(cd /', 'exec env')):
                    continue

                if line.strip().startswith('Use --sandbox_debug to see'):
                    continue

                if line.strip().startswith('# Configuration'):
                    continue

                # Ignore passing and skipped tests.
                if 'PASSED' in line or 'SKIPPED' in line:
                    continue

                # Ignore intermixed lines from other build steps.
                if re.match(r'\[\s*[\d,]+\s*/\s*[\d,]+\s*\]', line):
                    continue

                if re.match(r'Executed \d+ out of \d+ tests', line):
                    error_lines.append(line)
                    break

                error_lines.append(line)

    result = '\n'.join(error_lines)
    return re.sub(r'\n+', '\n', result)


if __name__ == '__main__':
    print(parse_bazel_stdout(Path(sys.argv[1])))
