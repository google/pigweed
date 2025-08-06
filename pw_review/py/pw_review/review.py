#!/usr/bin/env python3
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
"""Use Gemini CLI to review the commit at HEAD."""

from __future__ import annotations

import json
import logging
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import pw_cli.env
import pw_cli.git_repo
import pw_cli.tool_runner

_LOG = logging.getLogger(__name__)


class JsonExtractionError(Exception):
    pass


def _extract_json(gemini_output: str) -> dict:
    """Extracts and parses the JSON block from Gemini output."""
    # Gemini returns one JSON block contained inside triple backticks. Remove
    # those backticks to extract just the JSON part.
    match = re.search(
        r'```\s*(?:json)?(?P<json>.*?)```',
        gemini_output,
        re.DOTALL,
    )
    if not match:
        _LOG.error('Failed to find a JSON block in the Gemini output:')
        _LOG.error(gemini_output)
        raise JsonExtractionError(gemini_output)

    json_text = match.group('json')
    try:
        return json.loads(json_text)
    except json.JSONDecodeError as e:
        _LOG.error('Failed to parse the JSON response from Gemini: %s', e)
        _LOG.error(repr(gemini_output))
        _LOG.error(repr(json_text))
        raise


def _write_patch_file(diff_text: str, commit_hash: str) -> None:
    """Writes the diff to a patch file in a temporary directory."""
    patch = Path(
        tempfile.NamedTemporaryFile(
            prefix=commit_hash,
            suffix='.patch',
            delete=False,
        ).name,
    )
    patch.write_text(diff_text)
    print(f'Diff is saved to {patch}')


def review():
    """Use Gemini CLI to review the commit at HEAD."""
    _LOG.info('Creating a Gemini Code Review of the current commit at HEAD')
    _LOG.info('Staged and uncommitted changes are ignored.')

    runner = pw_cli.tool_runner.BasicSubprocessRunner()
    try:
        repo = pw_cli.git_repo.GitRepo(Path.cwd(), runner)
    except Exception:
        _LOG.error("Could not find Git repository root")
        raise

    commit_hash = repo.commit_hash(short=False)
    prompt_path = pw_cli.env.project_root() / '.gemini' / 'g-review_prompt.md'
    # TODO: b/426654921 - Fall back to Pigweed root if this file doesn't exist.

    _LOG.info('Reviewing changes for: %s', commit_hash)
    _LOG.info('%s', repo.commit_message().splitlines()[0])

    for line in repo.show(
        '--pretty=format:', '--name-status', commit_hash
    ).splitlines():
        _LOG.info('%s', line.strip())

    try:
        prompt_text = prompt_path.read_text()
    except FileNotFoundError:
        _LOG.error('%s not found', prompt_path)
        raise

    prompt = f'{prompt_text}\n{repo.show(commit_hash)}'

    try:
        proc = subprocess.run(
            ['gemini', '--prompt'],
            capture_output=True,
            check=True,
            input=prompt,
            text=True,
        )
    except FileNotFoundError:
        _LOG.error('gemini command not found, please add it to your path')
        raise
    except subprocess.CalledProcessError as e:
        _LOG.error('Gemini execution failed')
        _LOG.error('stdout:\n%s', e.stdout)
        _LOG.error('stderr:\n%s', e.stderr)
        raise

    json_response = _extract_json(proc.stdout)

    try:
        response_text = json_response['response_text']
        diff_text = json_response['diff']
    except KeyError as e:
        _LOG.error('Failed to parse the JSON response from Gemini: %s', e)
        _LOG.error(repr(json_response))
        raise

    print(response_text)

    if diff_text:
        print('Diff of suggested changes:')
        print(diff_text)
        _write_patch_file(diff_text, commit_hash)

    return 0


if __name__ == '__main__':
    sys.exit(review())
