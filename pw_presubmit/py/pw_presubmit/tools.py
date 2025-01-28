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
"""General purpose tools for running presubmit checks."""

import collections.abc
import logging
import os
from pathlib import Path
import shlex
import subprocess
from typing import (
    Iterable,
    Iterator,
    Sequence,
)

from pw_cli.tool_runner import ToolRunner
from pw_presubmit.presubmit_context import PRESUBMIT_CONTEXT

_LOG: logging.Logger = logging.getLogger(__name__)


def make_box(section_alignments: Sequence[str]) -> str:
    indices = [i + 1 for i in range(len(section_alignments))]
    top_sections = '{2}'.join('{1:{1}^{width%d}}' % i for i in indices)
    mid_sections = '{5}'.join(
        '{section%d:%s{width%d}}' % (i, section_alignments[i - 1], i)
        for i in indices
    )
    bot_sections = '{9}'.join('{8:{8}^{width%d}}' % i for i in indices)

    return ''.join(
        [
            '{0}',
            *top_sections,
            '{3}\n',
            '{4}',
            *mid_sections,
            '{6}\n',
            '{7}',
            *bot_sections,
            '{10}',
        ]
    )


def relative_paths(paths: Iterable[Path], start: Path) -> Iterable[Path]:
    """Returns relative Paths calculated with os.path.relpath."""
    for path in paths:
        yield Path(os.path.relpath(path, start))


def _truncate(value, length: int = 60) -> str:
    value = str(value)
    return (value[: length - 5] + '[...]') if len(value) > length else value


def format_command(args: Sequence, kwargs: dict) -> tuple[str, str]:
    attr = ', '.join(f'{k}={_truncate(v)}' for k, v in sorted(kwargs.items()))
    return attr, ' '.join(shlex.quote(str(arg)) for arg in args)


def log_run(
    args, ignore_dry_run: bool = False, **kwargs
) -> subprocess.CompletedProcess:
    """Logs a command then runs it with subprocess.run.

    Takes the same arguments as subprocess.run. The command is only executed if
    dry-run is not enabled.
    """
    ctx = PRESUBMIT_CONTEXT.get()
    if ctx:
        # Save the subprocess command args for pw build presubmit runner.
        if not ignore_dry_run:
            ctx.append_check_command(*args, **kwargs)
        if ctx.dry_run and not ignore_dry_run:
            # Return an empty CompletedProcess without actually running anything
            # if dry-run mode is on.
            empty_proc: subprocess.CompletedProcess = (
                subprocess.CompletedProcess('', 0)
            )
            empty_proc.stdout = b''
            empty_proc.stderr = b''
            return empty_proc
    _LOG.debug('[COMMAND] %s\n%s', *format_command(args, kwargs))
    return subprocess.run(args, **kwargs)


class PresubmitToolRunner(ToolRunner):
    """A simple ToolRunner that runs a process via `log_run()`."""

    @staticmethod
    def _custom_args() -> Iterable[str]:
        return ['pw_presubmit_ignore_dry_run']

    def _run_tool(
        self, tool: str, args, pw_presubmit_ignore_dry_run=False, **kwargs
    ) -> subprocess.CompletedProcess:
        """Run the requested tool as a subprocess."""
        return log_run(
            [tool, *args],
            **kwargs,
            ignore_dry_run=pw_presubmit_ignore_dry_run,
        )


def flatten(*items) -> Iterator:
    """Yields items from a series of items and nested iterables.

    This function is used to flatten arbitrarily nested lists. str and bytes
    are kept intact.
    """

    for item in items:
        if isinstance(item, collections.abc.Iterable) and not isinstance(
            item, (str, bytes, bytearray)
        ):
            yield from flatten(*item)
        else:
            yield item
