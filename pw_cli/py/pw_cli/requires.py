#!/usr/bin/env python3

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
"""Create transitive CLs for requirements on internal Gerrits.

This is only intended to be used by Googlers.

If the current CL needs to be tested alongside internal-project:1234 on an
internal project, but "internal-project" is something that can't be referenced
publicly, this automates creation of a CL on the pigweed-internal Gerrit that
references internal-project:1234 so the current commit effectively has a
requirement on internal-project:1234.

For more see http://go/pigweed-ci-cq-intro.
"""

import argparse
import dataclasses
import json
import logging
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Callable, IO, Sequence
import uuid

HELPER_GERRIT = 'pigweed-internal'
HELPER_PROJECT = 'requires-helper'
HELPER_REPO = 'sso://{}/{}'.format(HELPER_GERRIT, HELPER_PROJECT)

# Pass checks that look for "DO NOT ..." and block submission.
_DNS = ' '.join(
    (
        'DO',
        'NOT',
        'SUBMIT',
    )
)

# Subset of the output from pushing to Gerrit.
DEFAULT_OUTPUT = f'''
remote:
remote:   https://{HELPER_GERRIT}-review.git.corp.google.com/c/{HELPER_PROJECT}/+/123456789 {_DNS} [NEW]
remote:
'''.strip()

_LOG = logging.getLogger(__name__)


@dataclasses.dataclass
class Change:
    gerrit_name: str
    number: int


class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)


def dump_json_patches(obj: Sequence[Change], outs: IO):
    json.dump(obj, outs, indent=2, cls=EnhancedJSONEncoder)


def log_entry_exit(func: Callable) -> Callable:
    def wrapper(*args, **kwargs):
        _LOG.debug('entering %s()', func.__name__)
        _LOG.debug('args %r', args)
        _LOG.debug('kwargs %r', kwargs)
        try:
            res = func(*args, **kwargs)
            _LOG.debug('return value %r', res)
            return res
        except Exception as exc:
            _LOG.debug('exception %r', exc)
            raise
        finally:
            _LOG.debug('exiting %s()', func.__name__)

    return wrapper


@log_entry_exit
def parse_args() -> argparse.Namespace:
    """Creates an argument parser and parses arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'requirements',
        nargs='+',
        help='Requirements to be added ("<gerrit-name>:<cl-number>").',
    )
    parser.add_argument(
        '--push',
        action=argparse.BooleanOptionalAction,
        default=True,
        help=argparse.SUPPRESS,  # This option is only for debugging.
    )

    return parser.parse_args()


@log_entry_exit
def _run_command(*args, **kwargs) -> subprocess.CompletedProcess:
    kwargs.setdefault('capture_output', True)
    _LOG.debug('%s', args)
    _LOG.debug('%s', kwargs)
    res = subprocess.run(*args, **kwargs)
    _LOG.debug('%s', res.stdout)
    _LOG.debug('%s', res.stderr)
    res.check_returncode()
    return res


@log_entry_exit
def check_status() -> bool:
    res = subprocess.run(['git', 'status'], capture_output=True)
    if res.returncode:
        _LOG.error('repository not clean, commit to suppress this warning')
        return False
    return True


@log_entry_exit
def clone(requires_dir: Path) -> None:
    _LOG.info('cloning helper repository into %s', requires_dir)
    _run_command(['git', 'clone', HELPER_REPO, '.'], cwd=requires_dir)


@log_entry_exit
def create_commit(
    requires_dir: Path, requirement_strings: Sequence[str]
) -> None:
    """Create a commit in the local tree with the given requirements."""
    change_id = str(uuid.uuid4()).replace('-', '00')
    _LOG.debug('change_id %s', change_id)

    requirement_objects: list[Change] = []
    for req in requirement_strings:
        gerrit_name, number = req.split(':', 1)
        requirement_objects.append(Change(gerrit_name, int(number)))

    path = requires_dir / 'patches.json'
    _LOG.debug('path %s', path)
    with open(path, 'w') as outs:
        dump_json_patches(requirement_objects, outs)
        outs.write('\n')

    _run_command(['git', 'add', path], cwd=requires_dir)

    # TODO: b/232234662 - Don't add 'Requires:' lines to commit messages.
    commit_message = [
        f'{_DNS} {change_id[0:10]}\n\n',
        '',
        f'Change-Id: I{change_id}',
    ]
    for req in requirement_strings:
        commit_message.append(f'Requires: {req}')

    _LOG.debug('message %s', commit_message)
    _run_command(
        ['git', 'commit', '-m', '\n'.join(commit_message)],
        cwd=requires_dir,
    )

    # Not strictly necessary, only used for logging.
    _run_command(['git', 'show'], cwd=requires_dir)


@log_entry_exit
def push_commit(requires_dir: Path, push=True) -> Change:
    """Push a commit to the helper repository.

    Args:
        requires_dir: Local checkout of the helper repository.
        push: Whether to actually push or if this is a local-only test.

    Returns a Change object referencing the pushed commit.
    """

    output: str = DEFAULT_OUTPUT
    if push:
        res = _run_command(
            ['git', 'push', HELPER_REPO, '+HEAD:refs/for/main'],
            cwd=requires_dir,
        )
        output = res.stderr.decode()

    _LOG.debug('output: %s', output)
    regex = re.compile(
        f'^\\s*remote:\\s*'
        f'https://{HELPER_GERRIT}-review.(?:git.corp.google|googlesource).com/'
        f'c/{HELPER_PROJECT}/\\+/(?P<num>\\d+)\\s+',
        re.MULTILINE,
    )
    _LOG.debug('regex %r', regex)
    match = regex.search(output)
    if not match:
        raise ValueError(f"invalid output from 'git push': {output}")
    change_num = int(match.group('num'))
    _LOG.info('created %s change %s', HELPER_PROJECT, change_num)
    return Change(HELPER_GERRIT, change_num)


@log_entry_exit
def amend_existing_change(dependency: dict[str, str]) -> None:
    """Amend the current change to depend on the dependency

    Args:
        dependency: The change on which the top of the current checkout now
            depends.
    """
    git_root = Path(
        subprocess.run(
            ['git', 'rev-parse', '--show-toplevel'],
            capture_output=True,
        )
        .stdout.decode()
        .rstrip('\n')
    )
    patches_json = git_root / 'patches.json'
    _LOG.info('%s %d', patches_json, os.path.isfile(patches_json))

    patches = []
    if os.path.isfile(patches_json):
        with open(patches_json, 'r') as ins:
            patches = json.load(ins)

    patches.append(dependency)
    with open(patches_json, 'w') as outs:
        dump_json_patches(patches, outs)
        outs.write('\n')
    _LOG.info('%s %d', patches_json, os.path.isfile(patches_json))

    _run_command(['git', 'add', patches_json])
    _run_command(['git', 'commit', '--amend', '--no-edit'])


def run(requirements: Sequence[str], push: bool = True) -> int:
    """Entry point for requires."""

    if not check_status():
        return -1

    # Create directory for checking out helper repository.
    with tempfile.TemporaryDirectory() as requires_dir_str:
        requires_dir = Path(requires_dir_str)
        # Clone into helper repository.
        clone(requires_dir)
        # Make commit with requirements from command line.
        create_commit(requires_dir, requirements)
        # Push that commit and save its number.
        change = push_commit(requires_dir, push=push)
    # Add dependency on newly pushed commit on current commit.
    amend_existing_change(change)

    return 0


def main() -> int:
    return run(**vars(parse_args()))


if __name__ == '__main__':
    try:
        # If pw_cli is available, use it to initialize logs.
        from pw_cli import log

        log.install(logging.INFO)
    except ImportError:
        # If pw_cli isn't available, display log messages like a simple print.
        logging.basicConfig(format='%(message)s', level=logging.INFO)

    sys.exit(main())
