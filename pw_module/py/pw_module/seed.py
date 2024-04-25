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
"""Manages SEED documents in Pigweed."""

import argparse
import datetime
import enum
from dataclasses import dataclass
from pathlib import Path
import random
import re
import subprocess
import sys
import urllib.request
from typing import Dict, Iterable, List, Optional, Tuple, Union

import pw_cli.color
import pw_cli.env
from pw_cli.git_repo import GitRepo
from pw_cli.tool_runner import BasicSubprocessRunner

_NEW_SEED_TEMPLATE = '''.. _seed-{num:04d}:

{title_underline}
{formatted_title}
{title_underline}
.. seed::
   :number: {num}
   :name: {title}
   :status: {status}
   :proposal_date: {date}
   :cl: {changelist}
   :authors: {authors}
   :facilitator: Unassigned

-------
Summary
-------
Write up your proposal here.
'''


class SeedStatus(enum.Enum):
    """Possible states of a SEED proposal."""

    DRAFT = 0
    OPEN_FOR_COMMENTS = 1
    LAST_CALL = 2
    ACCEPTED = 3
    REJECTED = 4
    DEPRECATED = 5
    SUPERSEDED = 6
    ON_HOLD = 7
    META = 8

    def __str__(self) -> str:
        if self is SeedStatus.DRAFT:
            return 'Draft'
        if self is SeedStatus.OPEN_FOR_COMMENTS:
            return 'Open for Comments'
        if self is SeedStatus.LAST_CALL:
            return 'Last Call'
        if self is SeedStatus.ACCEPTED:
            return 'Accepted'
        if self is SeedStatus.REJECTED:
            return 'Rejected'
        if self is SeedStatus.DEPRECATED:
            return 'Deprecated'
        if self is SeedStatus.SUPERSEDED:
            return 'Superseded'
        if self is SeedStatus.ON_HOLD:
            return 'On Hold'
        if self is SeedStatus.META:
            return 'Meta'

        return ''


@dataclass
class SeedMetadata:
    number: int
    title: str
    authors: str
    status: SeedStatus
    changelist: Optional[int] = None
    sources: Optional[List[str]] = None

    def default_filename(self) -> str:
        normalized_title = self.title.lower().replace(' ', '-')
        normalized_title = re.sub(r'[^a-zA-Z0-9_-]', '', normalized_title)

        return f'{self.number:04d}-{normalized_title}.rst'


class SeedRegistry:
    """
    Represents a registry of SEEDs located somewhere in pigweed.git, which can
    be read, modified, and written.

    Currently, this is implemented as a basic text parser for the BUILD.gn file
    in the seed/ directory; however, in the future it may be rewritten to use a
    different backing data source.
    """

    class _State(enum.Enum):
        OUTER = 0
        SEED = 1
        INDEX = 2
        INDEX_SEEDS_LIST = 3

    @classmethod
    def parse(cls, registry_file: Path) -> 'SeedRegistry':
        return cls(registry_file)

    def __init__(self, seed_build_file: Path):
        self._file = seed_build_file
        self._lines = seed_build_file.read_text().split('\n')
        self._seeds: Dict[int, Tuple[int, int]] = {}
        self._next_seed_number = 101

        seed_regex = re.compile(r'pw_seed\("(\d+)"\)')

        state = SeedRegistry._State.OUTER

        section_start_index = 0
        current_seed_number = 0

        # Run through the GN file, doing some basic parsing of its targets.
        for i, line in enumerate(self._lines):
            if state is SeedRegistry._State.OUTER:
                seed_match = seed_regex.search(line)
                if seed_match:
                    # SEED definition target: extract the number.
                    state = SeedRegistry._State.SEED

                    section_start_index = i
                    current_seed_number = int(seed_match.group(1))

                    if current_seed_number >= self._next_seed_number:
                        self._next_seed_number = current_seed_number + 1

                if line == 'pw_seed_index("seeds") {':
                    state = SeedRegistry._State.INDEX
                    # Skip back past the comments preceding the SEED index
                    # target. New SEEDs will be inserted here.
                    insertion_index = i
                    while insertion_index > 0 and self._lines[
                        insertion_index - 1
                    ].startswith('#'):
                        insertion_index -= 1

                    self._seed_insertion_index = insertion_index

            if state is SeedRegistry._State.SEED:
                if line == '}':
                    self._seeds[current_seed_number] = (section_start_index, i)
                    state = SeedRegistry._State.OUTER

            if state is SeedRegistry._State.INDEX:
                if line == '}':
                    state = SeedRegistry._State.OUTER
                if line == '  seeds = [':
                    state = SeedRegistry._State.INDEX_SEEDS_LIST

            if state is SeedRegistry._State.INDEX_SEEDS_LIST:
                if line == '  ]':
                    self._index_seeds_end = i
                    state = SeedRegistry._State.INDEX

    def file(self) -> Path:
        """Returns the file which backs this registry."""
        return self._file

    def seed_count(self) -> int:
        """Returns the number of SEEDs registered."""
        return len(self._seeds)

    def insert(self, seed: SeedMetadata) -> None:
        """Adds a new seed to the registry."""

        new_seed = [
            f'pw_seed("{seed.number:04d}") {{',
            f'  title = "{seed.title}"',
            f'  author = "{seed.authors}"',
            f'  status = "{seed.status}"',
        ]

        if seed.changelist is not None:
            new_seed.append(f'  changelist = {seed.changelist}')

        if seed.sources is not None:
            if len(seed.sources) == 0:
                new_seed.append('  sources = []')
            elif len(seed.sources) == 1:
                new_seed.append(f'  sources = [ "{seed.sources[0]}" ]')
            else:
                new_seed.append('  sources = [')
                new_seed.extend(f'    "{source}",' for source in seed.sources)
                new_seed.append('  ]')

        new_seed += [
            '}',
            '',
        ]
        self._lines = (
            self._lines[: self._seed_insertion_index]
            + new_seed
            + self._lines[self._seed_insertion_index : self._index_seeds_end]
            + [f'    ":{seed.number:04d}",']
            + self._lines[self._index_seeds_end :]
        )

        self._seed_insertion_index += len(new_seed)
        self._index_seeds_end += len(new_seed)

        if seed.number == self._next_seed_number:
            self._next_seed_number += 1

    def next_seed_number(self) -> int:
        return self._next_seed_number

    def write(self) -> None:
        self._file.write_text('\n'.join(self._lines))


_GERRIT_HOOK_URL = (
    'https://gerrit-review.googlesource.com/tools/hooks/commit-msg'
)


# TODO: pwbug.dev/318746837 - Extract this to somewhere more general.
def _install_gerrit_hook(git_root: Path) -> None:
    hook_file = git_root / '.git' / 'hooks' / 'commit-msg'
    urllib.request.urlretrieve(_GERRIT_HOOK_URL, hook_file)
    hook_file.chmod(0o755)


def _request_new_seed_metadata(
    repo: GitRepo,
    registry: SeedRegistry,
    colors,
) -> SeedMetadata:
    if repo.has_uncommitted_changes():
        print(
            colors.red(
                'You have uncommitted Git changes. '
                'Please commit or stash before creating a SEED.'
            )
        )
        sys.exit(1)

    print(
        colors.yellow(
            'This command will create Git commits. '
            'Make sure you are on a suitable branch.'
        )
    )
    print(f'Current branch: {colors.cyan(repo.current_branch())}')
    print('')

    number = registry.next_seed_number()

    try:
        num = input(
            f'SEED number (default={colors.bold_white(number)}): '
        ).strip()

        while True:
            try:
                if num:
                    number = int(num)
                break
            except ValueError:
                num = input('Invalid number entered. Try again: ').strip()

        title = input('SEED title: ').strip()
        while not title:
            title = input(
                'Title cannot be empty. Re-enter SEED title: '
            ).strip()

        authors = input('SEED authors: ').strip()
        while not authors:
            authors = input(
                'Authors list cannot be empty. Re-enter SEED authors: '
            ).strip()

        print('The following SEED will be created.')
        print('')
        print(f'  Number: {colors.green(number)}')
        print(f'  Title: {colors.green(title)}')
        print(f'  Authors: {colors.green(authors)}')
        print('')
        print(
            'This will create two commits on branch '
            + colors.cyan(repo.current_branch())
            + ' and push them to Gerrit.'
        )

        create = True
        confirm = input(f'Proceed? [{colors.bold_white("Y")}/n] ').strip()
        if confirm:
            create = confirm == 'Y'

    except KeyboardInterrupt:
        print('\nReceived CTRL-C, exiting...')
        sys.exit(0)

    if not create:
        sys.exit(0)

    return SeedMetadata(
        number=number,
        title=title,
        authors=authors,
        status=SeedStatus.DRAFT,
    )


@dataclass
class GerritChange:
    id: str
    number: int
    title: str

    def url(self) -> str:
        return (
            'https://pigweed-review.googlesource.com'
            f'/c/pigweed/pigweed/+/{self.number}'
        )


def commit_and_push(
    repo: GitRepo,
    files: Iterable[Union[Path, str]],
    commit_message: str,
    change_id: Optional[str] = None,
) -> GerritChange:
    """Creates a commit with the given files and message and pushes to Gerrit.

    Args:
        change_id: Optional Gerrit change ID to use. If not specified, generates
            a new one.
    """
    if change_id is not None:
        commit_message = f'{commit_message}\n\nChange-Id: {change_id}'

    subprocess.run(
        ['git', 'add'] + list(files), capture_output=True, check=True
    )
    subprocess.run(
        ['git', 'commit', '-m', commit_message], capture_output=True, check=True
    )

    if change_id is None:
        # Parse the generated change ID from the commit if it wasn't
        # explicitly set.
        change_id = repo.commit_change_id()

        if change_id is None:
            # If the commit doesn't have a Change-Id, the Gerrit hook is not
            # installed. Install it and try modifying the commit.
            _install_gerrit_hook(repo.root())
            subprocess.run(
                ['git', 'commit', '--amend', '--no-edit'],
                capture_output=True,
                check=True,
            )
            change_id = repo.commit_change_id()
            assert change_id is not None

    process = subprocess.run(
        [
            'git',
            'push',
            'origin',
            '+HEAD:refs/for/main',
            '--no-verify',
        ],
        capture_output=True,
        text=True,
        check=True,
    )

    output = process.stderr

    regex = re.compile(
        '^\\s*remote:\\s*'
        'https://pigweed-review.(?:git.corp.google|googlesource).com/'
        'c/pigweed/pigweed/\\+/(?P<num>\\d+)\\s+',
        re.MULTILINE,
    )
    match = regex.search(output)
    if not match:
        raise ValueError(f"invalid output from 'git push': {output}")
    change_num = int(match.group('num'))

    return GerritChange(change_id, change_num, commit_message.split('\n')[0])


def _create_wip_seed_doc_change(
    repo: GitRepo,
    new_seed: SeedMetadata,
) -> GerritChange:
    """Commits and pushes a boilerplate CL doc to Gerrit.

    Returns information about the CL.
    """
    env = pw_cli.env.pigweed_environment()
    seed_rst_file = env.PW_ROOT / 'seed' / new_seed.default_filename()

    formatted_title = f'{new_seed.number:04d}: {new_seed.title}'
    title_underline = '=' * len(formatted_title)

    seed_rst_file.write_text(
        _NEW_SEED_TEMPLATE.format(
            formatted_title=formatted_title,
            title_underline=title_underline,
            num=new_seed.number,
            title=new_seed.title,
            authors=new_seed.authors,
            status=new_seed.status,
            date=datetime.date.today().strftime('%Y-%m-%d'),
            changelist=0,
        )
    )

    temp_branch = f'wip-seed-{new_seed.number}-{random.randrange(0, 2**16):x}'
    subprocess.run(
        ['git', 'checkout', '-b', temp_branch], capture_output=True, check=True
    )

    commit_message = f'SEED-{new_seed.number:04d}: {new_seed.title}'

    try:
        cl = commit_and_push(repo, [seed_rst_file], commit_message)
    except subprocess.CalledProcessError as err:
        print(f'Command {err.cmd} failed; stderr:')
        print(err.stderr)
        sys.exit(1)
    finally:
        subprocess.run(
            ['git', 'checkout', '-'], capture_output=True, check=True
        )
        subprocess.run(
            ['git', 'branch', '-D', temp_branch],
            capture_output=True,
            check=True,
        )

    return cl


def create_seed_number_claim_change(
    repo: GitRepo,
    new_seed: SeedMetadata,
    registry: SeedRegistry,
) -> GerritChange:
    commit_message = f'SEED-{new_seed.number:04d}: Claim SEED number'
    registry.insert(new_seed)
    registry.write()
    return commit_and_push(repo, [registry.file()], commit_message)


def _create_seed_doc_change(
    repo: GitRepo,
    new_seed: SeedMetadata,
    registry: SeedRegistry,
    wip_change: GerritChange,
) -> None:
    env = pw_cli.env.pigweed_environment()
    seed_rst_file = env.PW_ROOT / 'seed' / new_seed.default_filename()

    formatted_title = f'{new_seed.number:04d}: {new_seed.title}'
    title_underline = '=' * len(formatted_title)

    seed_rst_file.write_text(
        _NEW_SEED_TEMPLATE.format(
            formatted_title=formatted_title,
            title_underline=title_underline,
            num=new_seed.number,
            title=new_seed.title,
            authors=new_seed.authors,
            status=new_seed.status,
            date=datetime.date.today().strftime('%Y-%m-%d'),
            changelist=new_seed.changelist,
        )
    )

    new_seed.sources = [seed_rst_file.relative_to(registry.file().parent)]
    new_seed.changelist = None
    registry.insert(new_seed)
    registry.write()

    commit_message = f'SEED-{new_seed.number:04d}: {new_seed.title}'
    commit_and_push(
        repo,
        [registry.file(), seed_rst_file],
        commit_message,
        change_id=wip_change.id,
    )


def create_seed() -> int:
    colors = pw_cli.color.colors()
    env = pw_cli.env.pigweed_environment()

    repo = GitRepo(env.PW_ROOT, BasicSubprocessRunner())

    registry_path = env.PW_ROOT / 'seed' / 'BUILD.gn'

    wip_registry = SeedRegistry.parse(registry_path)
    registry = SeedRegistry.parse(registry_path)

    seed = _request_new_seed_metadata(repo, wip_registry, colors)
    seed_cl = _create_wip_seed_doc_change(repo, seed)

    seed.changelist = seed_cl.number

    number_cl = create_seed_number_claim_change(repo, seed, wip_registry)
    _create_seed_doc_change(repo, seed, registry, seed_cl)

    print()
    print(f'Created two CLs for SEED-{seed.number:04d}:')
    print()
    print(f'-  {number_cl.title}')
    print(f'   <{number_cl.url()}>')
    print()
    print(f'-  {seed_cl.title}')
    print(f'   <{seed_cl.url()}>')

    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.set_defaults(func=lambda **_kwargs: parser.print_help())

    subparsers = parser.add_subparsers(title='subcommands')

    # No args for now, as this initially only runs in interactive mode.
    create_parser = subparsers.add_parser('create', help='Creates a new SEED')
    create_parser.set_defaults(func=create_seed)

    return parser.parse_args()


def main() -> int:
    args = {**vars(parse_args())}
    func = args['func']
    del args['func']

    exit_code = func(**args)
    return 0 if exit_code is None else exit_code


if __name__ == '__main__':
    sys.exit(main())
