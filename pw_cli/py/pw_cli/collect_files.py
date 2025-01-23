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
"""Utilities for file collection in a repository."""

import logging
from pathlib import Path
from typing import Collection, Pattern, Sequence

from pw_cli.tool_runner import ToolRunner
from pw_cli.file_filter import exclude_paths
from pw_cli.git_repo import (
    describe_git_pattern,
    find_git_repo,
    GitError,
    TRACKING_BRANCH_ALIAS,
)


_LOG = logging.getLogger(__name__)


def collect_files_in_current_repo(
    pathspecs: Collection[Path | str],
    tool_runner: ToolRunner,
    modified_since_git_ref: str | None = None,
    exclude_patterns: Collection[Pattern] = tuple(),
    action_flavor_text: str = 'Collecting',
) -> Sequence[Path]:
    """Collects files given a variety of pathspecs and maps them to their repo.

    This is a relatively fuzzy file finder for projects tracked in a Git repo.
    It's designed to adhere to the following constraints:

      * If a pathspec is a real file, unconditionally return it.
      * If no pathspecs are passed, collect from the current working directory.
      * Return the path of any files modified since `modified_since_git_ref`
        (which may be a branch, tag, or commit) that match the provided
        pathspecs.
      * Passing no pathspecs has the same behavior as passing `.` (everything in
        the current directory).

    Args:
        pathspecs: Files or git pathspecs to collect files from. Wildcards (e.g.
            `pw_cl*`) are accepted.
        tool_runner: The ToolRunner to use for Git operations.
        modified_since_git_ref: If the passed pathspec is tracked by a git repo,
            it is excluded if unmodified since the specified pathspec. If the
            pathspec is `None`, no files are excluded.
        exclude_patterns: A collection of exclude patterns to exclude from the
            set of collected files.
        action_flavor_text: Replaces "Collecting" in the
            "Collecting all files in the foo repo" log message with a
            personalized string (e.g. "Formatting all files...").

    Returns:
        A dictionary mapping a GitRepo to a list of paths relative to that
        repo's root that match the provided pathspecs. Files not tracked by
        any git repo are mapped to the `None` key.
    """
    # TODO: https://pwbug.dev/391690594 - This is brittle and not covered by
    # tests. Someday it should be re-thought, particularly to better handle
    # multi-repo setups.
    files = [Path(path).resolve() for path in pathspecs if Path(path).is_file()]
    try:
        current_repo = find_git_repo(Path.cwd(), tool_runner)
    except GitError:
        current_repo = None

    # If this is a Git repo, list the original paths with git ls-files or diff.
    if current_repo is not None:
        # Implement a graceful fallback in case the tracking branch isn't
        # available.
        if (
            modified_since_git_ref == TRACKING_BRANCH_ALIAS
            and not current_repo.tracking_branch()
        ):
            _LOG.warning(
                'Failed to determine the tracking branch, using --base HEAD~1 '
                'instead of listing all files'
            )
            modified_since_git_ref = 'HEAD~1'

        _LOG.info(
            '%s %s',
            action_flavor_text,
            describe_git_pattern(
                Path.cwd(),
                modified_since_git_ref,
                pathspecs,
                exclude_patterns,
                tool_runner,
                current_repo.root(),
            ),
        )

        # Add files from Git and remove duplicates.
        files = sorted(
            set(
                exclude_paths(
                    exclude_patterns,
                    current_repo.list_files(modified_since_git_ref, pathspecs),
                )
            )
            | set(files)
        )
    elif modified_since_git_ref:
        _LOG.critical(
            'A base commit may only be provided if running from a Git repo'
        )

    return files
