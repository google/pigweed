# Copyright 2024 The Pigweed Authors
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
"""
Helpful commands for working with the repo tool.
See https://gerrit.googlesource.com/git-repo/ for more info!
"""

import subprocess

from typing import List
from pathlib import Path
from pw_presubmit.tools import log_run


def list_all_git_repos() -> List[Path]:
    """Query repo tool and return a list of git repos in the current project.

    Returns:
        List of "Path"s which were found.
    """
    repos = (
        log_run(
            ['repo', 'forall', '-c', 'git', 'rev-parse', '--show-toplevel'],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=True,
            ignore_dry_run=True,
        )
        .stdout.decode()
        .strip()
    )

    return [Path(line) for line in repos.splitlines()]
