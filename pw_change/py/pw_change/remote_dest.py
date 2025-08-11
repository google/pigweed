# Copyright 2025 The Pigweed Authors
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
"""Get the remote and destination branch for a change."""

import subprocess


def remote_dest() -> tuple[str, str]:
    remote_branch = (
        subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            capture_output=True,
            check=True,
        )
        .stdout.decode()
        .strip()
    )
    while '/' not in remote_branch:
        cmd = [
            'git',
            'rev-parse',
            '--abbrev-ref',
            '--symbolic-full-name',
            f'{remote_branch}@{{upstream}}',
        ]
        remote_branch = (
            subprocess.run(
                cmd,
                capture_output=True,
                check=True,
            )
            .stdout.decode()
            .strip()
        )

    remote, branch = remote_branch.split('/', 1)
    return remote, branch
