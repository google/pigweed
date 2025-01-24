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
"""Supporting helpers for format CLI tooling."""

import logging
from pathlib import Path
import sys
from typing import (
    Sequence,
    TextIO,
)

from pw_cli import color
from pw_cli.diff import colorize_diff
from pw_presubmit.format.core import FormattedDiff

_LOG: logging.Logger = logging.getLogger(__name__)


def findings_to_formatted_diffs(
    diffs: dict[Path, str]
) -> Sequence[FormattedDiff]:
    """Converts legacy formatter findings to structured findings."""
    return [
        FormattedDiff(
            ok=True,
            diff=finding,
            error_message=None,
            file_path=path,
        )
        for path, finding in diffs.items()
    ]


def summarize_findings(
    findings: Sequence[FormattedDiff],
    log_fix_command: bool,
    log_oneliner_summary: bool,
    file: TextIO = sys.stdout,
) -> None:
    """Prints a summary of a format check's findings."""
    if not findings:
        return

    if log_oneliner_summary:
        _LOG.warning(
            'Found %d files with formatting issues:',
            len(findings),
        )

    paths_to_fix = []
    for formatting_finding in findings:
        if not formatting_finding.diff:
            continue

        paths_to_fix.append(formatting_finding.file_path)
        diff = (
            colorize_diff(formatting_finding.diff)
            if color.is_enabled(file)
            else formatting_finding.diff
        )
        file.write(diff)

    if log_fix_command:
        # TODO: https://pwbug.dev/326309165 - Add a Bazel-specific command.
        format_command = "pw format --fix"

        def path_relative_to_cwd(path: Path):
            try:
                return Path(path).resolve().relative_to(Path.cwd().resolve())
            except ValueError:
                return Path(path).resolve()

        paths = " ".join([str(path_relative_to_cwd(p)) for p in paths_to_fix])
        message = f'  {format_command} {paths}'
        _LOG.warning('To fix formatting, run:\n\n%s\n', message)
