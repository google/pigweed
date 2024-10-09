# Copyright 2023 The Pigweed Authors
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
"""Generates BUILD.gn files from rules in Bazel workspace."""

import argparse
import json
import os
import re

from collections import defaultdict, deque
from pathlib import Path, PurePath, PurePosixPath
from typing import (
    Deque,
    IO,
    Iterable,
    Iterator,
    Set,
)

from pw_build.bazel_query import (
    ParseError,
    BazelLabel,
    BazelRule,
    BazelWorkspace,
)
from pw_build.gn_target import GnTarget
from pw_build.gn_writer import GnFile


class BazelToGnConverter:
    """Manages the conversion of Bazel rules into GN targets."""

    def __init__(self, pw_root: Path) -> None:
        """Instantiates a Bazel workspace.

        Args:
            pw_root: Path to Pigweed directory, e.g. "$PW_ROOT".
        """
        self._names_by_repo: dict[str, str] = {}
        self._names_by_build_arg: dict[str, str] = {}
        self._pending: Deque[BazelLabel] = deque()
        self._loaded: Set[str] = set()
        self._source_dirs: dict[str, PurePath] = {
            'pigweed': pw_root,
        }
        self._workspaces: dict[str, BazelWorkspace] = {
            'pigweed': BazelWorkspace(
                'com_google_pigweed', pw_root, fetch=False
            ),
        }
        self._revisions: dict[str, str] = {}

    def get_name(
        self,
        label: BazelLabel | None = None,
        repo: str | None = None,
        build_arg: str | None = None,
    ) -> str:
        """Returns the name of a third-party module.

        Exactly one of the "label", "repo" or "build_arg" keyword arguments must
        be provided.

        Args:
            label:      Bazel label referring to the third party module.
            repo:       Bazel repository of the third party module,
                        e.g. "com_google_foo".
            build_arg:  GN build argument of the third party module,
                        e.g. "$dir_pw_third_party_foo".
        """
        if label:
            assert not repo, 'multiple keyword arguments provided'
            repo = label.repo()
        assert not repo or not build_arg, 'multiple keyword arguments provided'
        try:
            if repo:
                return self._names_by_repo[repo]
            if build_arg:
                return self._names_by_build_arg[build_arg]
            raise AssertionError('no keyword arguments provided')
        except KeyError as e:
            raise ParseError(
                f'unrecognized third party module: "{e.args[0]}"; '
                'does it have a bazel_to_gn.json file?'
            )

    def get_source_dir(self, name: str) -> PurePath:
        """Returns the source directory for a third-party module.

        Args:
            name: Name of the third party module.
        """
        build_arg = self._build_arg(name)
        source_dir = self._source_dirs.get(build_arg)
        if not source_dir:
            raise KeyError(f'GN build argument not set: "{build_arg}"')
        return source_dir

    def parse_args_gn(self, file: IO) -> None:
        """Reads third party build arguments from args.gn.

        Args:
            file: File-like object to read from.
        """
        build_arg_pat = r'(dir_pw_third_party_\S*)\s*=\s*"([^"]*)"'
        for line in file:
            match = re.search(build_arg_pat, line)
            if match:
                build_arg = f'${match.group(1)}'
                source_dir = PurePath(match.group(2))
                self._source_dirs[build_arg] = source_dir

    def load_workspace(self, name: str, bazel_to_gn: IO) -> None:
        """Parses a bazel_to_gn.json file and loads the workspace it describes.

        Recognized fields include:
            repo:     The Bazel name of the repository.
            generate: Disables generating GN if present and set to `False`.
            targets:  A list of Bazel labels to generate GN for.
            options:  A dictionary of mapping Bazel flags to build settings.

        Args:
            name: Name of a third party module.
            bazel_to_gn: A file-like object describing the Bazel workspace.
        """
        json_data = json.load(bazel_to_gn)
        generate = json_data.get('generate', True)
        source_dir = None
        if generate:
            source_dir = self.get_source_dir(name)
        repo = json_data['repo']
        workspace = BazelWorkspace(repo, source_dir)
        workspace.generate = generate
        workspace.targets = json_data.get('targets', [])
        workspace.options = json_data.get('options', {})
        self._names_by_repo[repo] = name
        self._workspaces[name] = workspace

    def get_initial_targets(self, name: str) -> list[BazelLabel]:
        """Adds labels from a third party module to the converter queue.

        Returns the number of labels added.

        Args:
            name: Name of a previously loaded repo.
        """
        workspace = self._workspaces[name]
        repo = workspace.repo()
        self._loaded = set(workspace.targets)
        return [BazelLabel(short, repo=repo) for short in self._loaded]

    def pending(self) -> Iterable[BazelLabel]:
        """Returns the label for the next rule that needs to be loaed."""
        while self._pending:
            label = self._pending.popleft()
            if str(label) in self._loaded:
                continue
            self._loaded.add(str(label))
            yield label

    def load_rules(self, labels: list[BazelLabel]) -> Iterable[BazelRule]:
        """Queries a Bazel workspace to instantiate a rule.

        Return `None` if the GN files for the workspace are manually
        generated, otherwise returns the rule. Adds the rules deps to the queue
        of pending labels to be loaded.

        Args:
            label:  The Bazel label indicating the workspace and target.
        """
        by_repo: dict[str, list[BazelLabel]] = defaultdict(list)
        deps: Set[str] = set()
        for label in labels:
            by_repo[label.repo()].append(label)
        for repo, labels_for_repo in by_repo.items():
            name = self.get_name(repo=repo)
            workspace = self._workspaces[name]
            for rule in workspace.get_rules(labels_for_repo):
                label = rule.label()
                package = label.package()
                for attr_name in ['deps', 'implementation_deps']:
                    for dep in rule.get_list(attr_name):
                        label = BazelLabel(dep, repo=repo, package=package)
                        deps.add(str(label))
                yield rule
        self._pending.extend([BazelLabel(dep) for dep in deps])

    def package(self, rule: BazelRule) -> str:
        """Returns the relative path to the BUILD.gn corresponding to a rule.

        The relative path is relative to $dir_pw_third_party, and consists of
        the third party module name and the package portion of the Bazel label.

        Args:
            rule: The rule to get the relative path for.
        """
        label = rule.label()
        name = self.get_name(label=label)
        return f'{name}/{label.package()}'

    def convert_rule(self, rule: BazelRule) -> GnTarget:
        """Creates a GN target from a Bazel rule.

        Args:
            rule: The rule to convert into a GnTarget.
        """
        label = rule.label()
        name = self.get_name(label=label)
        if rule.kind() == 'cc_library':
            if rule.get_bool('linkstatic'):
                target_type = f'{name}_static_library'.replace('-', '_')
            else:
                target_type = f'{name}_source_set'.replace('-', '_')
        else:
            raise ParseError(f'unsupported Bazel kind: {rule.kind()}')
        gn_target = GnTarget(target_type, label.target())
        gn_target.origin = str(label)
        gn_target.attrs = {
            'public': list(self._source_relative(name, rule, 'hdrs')),
            'sources': list(self._source_relative(name, rule, 'srcs')),
            'inputs': list(
                self._source_relative(name, rule, 'additional_linker_inputs')
            ),
            'include_dirs': list(self._source_relative(name, rule, 'includes')),
            'cflags': rule.get_list('copts'),
            'public_defines': rule.get_list('defines'),
            'ldflags': rule.get_list('linkopts'),
            'defines': rule.get_list('local_defines'),
            'public_deps': list(self._build_relative(name, rule, 'deps')),
            'deps': list(
                self._build_relative(name, rule, 'implementation_deps')
            ),
        }

        return gn_target

    def num_loaded(self) -> int:
        """Returns the number of rules loaded thus far."""
        return len(self._loaded)

    def get_workspace_revisions(self) -> Iterable[str]:
        """Returns the revisions needed by each generated workspace."""
        for name, workspace in self._workspaces.items():
            if name == 'pigweed':
                continue
            if workspace.generate:
                yield f'{name:<16}: {workspace.revision()}'

    def update_pw_package(
        self, name: str, lines: Iterator[str]
    ) -> Iterable[str]:
        """Updates the third party package revision in the pw_package module.

        Args:
            lines: Contents of the existing pw_package package file.
        """
        workspace = self._workspaces[name]
        if name in self._revisions:
            revision = self._revisions[name]
        else:
            revision = workspace.revision('HEAD')
        for line in lines:
            line = line.rstrip()
            m = re.match(r'(.*commit=[\'"])([a-z0-9]*)([\'"],.*)', line)
            if not m:
                yield line
                continue
            current = m.group(2)
            if workspace.timestamp(current) < workspace.timestamp(revision):
                yield f'{m.group(1)}{revision}{m.group(3)}'
            else:
                yield line
        yield ''

    def get_imports(self, gn_target: GnTarget) -> Iterable[str]:
        """Returns the GNI files needed by the given target."""
        for build_arg in gn_target.build_args():
            name = self.get_name(build_arg=build_arg)
            yield f'$dir_pw_third_party/{name}/{name}.gni'

    def update_doc_rst(self, name: str, lines: Iterator[str]) -> Iterable[str]:
        """Replaces the "Version" part of docs.rst with the latest revision.

        This will truncate everything after the "generated section" comment and
        add the comment and version information. If the file does not have the
        comment, the comment and information will appended to the end of the
        file.

        Args:
            lines: Iterator of lines.
        """
        workspace = self._workspaces[name]
        comment = '.. DO NOT EDIT BELOW THIS LINE. Generated section.'
        url = workspace.url().rstrip('.git')
        revision = workspace.revision()
        short = revision[:8]
        for line in lines:
            line = line.rstrip()
            if line == comment:
                break
            yield line
        yield comment
        yield ''
        yield 'Version'
        yield '======='
        yield f'The update script was last run for revision `{short}`_.'
        yield ''
        yield f'.. _{short}: {url}/tree/{revision}'
        yield ''

    def _build_arg(self, name: str) -> str:
        """Returns the GN build argument for a third party module."""
        build_arg = f'$dir_pw_third_party_{name}'.replace('-', '_')
        if build_arg not in self._names_by_build_arg:
            self._names_by_build_arg[build_arg] = name
        return build_arg

    def _source_relative(
        self, name: str, rule: BazelRule, attr_name: str
    ) -> Iterable[str]:
        """Provides GN paths relative to the third party source directory."""
        if not rule.has_attr(attr_name):
            return
        attr_type = rule.attr_type(attr_name)
        build_arg = self._build_arg(name)
        repo = rule.label().repo()
        if attr_type == 'string_list':
            for item in rule.get_list(attr_name):
                yield f'{build_arg}/{item}'
        elif attr_type == 'label_list':
            for item in rule.get_list(attr_name):
                label = BazelLabel(item, repo=repo)
                yield f'{build_arg}/{label.package()}/{label.target()}'
        else:
            raise ParseError(f'unknown attribute type: {attr_type}')

    def _build_relative(
        self, name: str, rule: BazelRule, attr_name: str
    ) -> Iterable[str]:
        """Provides GN labels relative to the directory under //third_party."""
        label = rule.label()
        repo = label.repo()
        for other_str in rule.get_list(attr_name):
            other = BazelLabel(other_str, repo=repo, package=label.package())
            package = f'{name}/{label.package()}'
            other_package = f'{self.get_name(label=other)}/{other.package()}'

            # Abbreviate the label only if it is part of the same repo.
            if label.repo() != other.repo():
                path = PurePosixPath('$dir_pw_third_party', other_package)
            elif other_package == package:
                path = None
            else:
                path = PurePosixPath(package)
                other_path = PurePosixPath(other_package)
                common = PurePosixPath(
                    *os.path.commonprefix([path.parts, other_path.parts])
                )
                walk_up = PurePosixPath(
                    *(['..'] * (len(path.parts) - len(common.parts)))
                )
                walk_down = other_path.relative_to(common)
                path = PurePosixPath(walk_up, walk_down)

            if not path:
                yield f':{other.target()}'
            elif path.name == other.target():
                yield f'{path}'
            else:
                yield f'{path}:{other.target()}'

    def _get_http_archives(self) -> dict[str, BazelRule]:
        """Returns a mapping of third party modules to rules.

        The returned rules described the most recently required version of the
        third party module.
        """
        # First, examine http_archives in the third_party workspaces.
        http_archives = {}
        for name, workspace in self._workspaces.items():
            if name == 'pigweed':
                continue
            if not workspace.generate:
                continue
            for rule in workspace.get_http_archives():
                repo = rule.label().target()
                if repo not in self._names_by_repo:
                    continue
                other_name = self._names_by_repo[repo]
                other = self._workspaces[other_name]
                if not other.generate:
                    continue
                tag = rule.get_str('strip_prefix').replace(f'{other_name}-', '')
                revision = other.revision(tag)
                timestamp = other.timestamp(revision)
                if other_name in self._revisions:
                    strictest = other.timestamp(self._revisions[other_name])
                    keep = strictest < timestamp
                else:
                    keep = True
                if keep:
                    http_archives[repo] = rule
                    self._revisions[other_name] = revision

        # Next, compare them to those in the WORKSPACE file.
        pigweed = self._workspaces['pigweed']
        for rule in pigweed.get_http_archives():
            repo = rule.label().target()
            if repo not in self._names_by_repo:
                continue
            name = self._names_by_repo[repo]
            workspace = self._workspaces[name]
            if not workspace.generate:
                continue
            if name not in self._revisions:
                old_rev = rule.get_str('strip_prefix').replace(f'{name}-', '')
                new_rev = workspace.revision('HEAD')
                rule.set_attr('strip_prefix', f'{name}-{new_rev}')
                if rule.has_attr('url'):
                    url = rule.get_str('url')
                    rule.set_attr('url', url.replace(old_rev, new_rev))
                if rule.has_attr('urls'):
                    urls = rule.get_list('urls')
                    urls = [url.replace(old_rev, new_rev) for url in urls]
                    rule.set_attr('urls', urls)
                keep = True
            else:
                tag = rule.get_str('strip_prefix').replace(f'{name}-', '')
                new_rev = workspace.revision(tag)
                timestamp = workspace.timestamp(new_rev)
                strictest = workspace.timestamp(self._revisions[name])
                keep = strictest < timestamp
            if keep:
                http_archives[repo] = rule
                self._revisions[name] = new_rev

        # Next, check that the current revisions satisfy the strict revisions.
        for name, workspace in self._workspaces.items():
            if name not in self._revisions:
                continue
            needed = workspace.timestamp(self._revisions[name])
            actual = workspace.timestamp('HEAD')
            if actual < needed:
                raise RuntimeError(f'{name} must be from after {needed}.')

        # Finally, return the mapping.
        return http_archives


def _parse_args() -> argparse.Namespace:
    """Parse arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-b',
        '--build_dir',
        type=PurePath,
        help=('Build output directory, which must contain "args.gn"'),
    )
    parser.add_argument(
        'names',
        type=str,
        nargs='+',
        help=(
            'Third-party dependencies to generate GN for. '
            'Must match a subdirectoy of $PW_ROOT/third_party'
        ),
    )
    args = parser.parse_args()

    if not args.build_dir:
        pw_root = os.getenv('PW_ROOT')
        if not pw_root:
            raise RuntimeError('PW_ROOT is not set')
        args.build_dir = PurePath(pw_root, 'out')

    if not args.build_dir.is_absolute():
        args.build_dir = args.pw_root.joinpath(args.build_dir)

    return args


def _overprint(msg: str) -> None:
    """Prints with a carriage return instead of a newline."""
    print(msg.ljust(80), end='\r', flush=True)


def _bazel_to_gn(args: argparse.Namespace) -> None:
    """Generates BUILD.gn files from rules in Bazel workspace.

    This script is intended to be as unit-testable as possible. As a result,
    most functionality has been pushed into testable methods of
    BazelToGnConverter.

    This method primarily consists of three things:
      1. Print statements to provide feedback to the user.
      2. File operations, to make subroutines more unit testable.
      3. Control flow and loops around the two previous categories.

    Args:
        args: Script arguments. See `_parse_args`.
    """
    build_dir = Path(args.build_dir)
    b2g = BazelToGnConverter(build_dir.parent)

    args_gn_path = build_dir.joinpath('args.gn')
    print(f'Reading build arguments from {args_gn_path}...')
    with open(args_gn_path) as args_gn:
        b2g.parse_args_gn(args_gn)

    print('Converting Bazel rules and their dependencies to GN targets...')
    third_party_path = Path(build_dir.parent, 'third_party')
    for child in third_party_path.iterdir():
        try:
            if child.is_dir():
                with open(child.joinpath('bazel_to_gn.json')) as file:
                    b2g.load_workspace(child.name, file)
                print(f'Bazel workspace loaded for //third_party/{child.name}')
        except FileNotFoundError:
            pass

    print('Starting from:')
    for name in args.names:
        try:
            labels = b2g.get_initial_targets(name)
        except KeyError:
            print(f'E: Unable to get initial targets for "{name}".')
            print(f'E: Is "//third_party/{name}/bazel_to_gn.json" missing?')
            return
        print(f'  {len(labels)} initial rule(s) in {name}')

    by_package: dict[str, list[GnTarget]] = defaultdict(list)
    while labels:
        for rule in b2g.load_rules(labels):
            by_package[b2g.package(rule)].append(b2g.convert_rule(rule))
            _overprint(f'[{b2g.num_loaded()}] {rule.label()}')
        labels = list(b2g.pending())
    print(f'[{b2g.num_loaded()}] Conversion complete!'.ljust(80))

    for package, gn_targets in sorted(by_package.items()):
        build_gn_path = third_party_path.joinpath(package, 'BUILD.gn')
        imports = set().union(
            *[b2g.get_imports(gn_target) for gn_target in gn_targets]
        )
        _overprint(f'Writing {build_gn_path}...')
        with GnFile(build_gn_path) as build_gn:
            build_gn.write_file(imports, gn_targets)

    names = {package.split('/')[0] for package in by_package.keys()}

    for name in names:
        update_path = build_dir.parent.joinpath(
            'pw_package',
            'py',
            'pw_package',
            'packages',
            name.replace('-', '_') + '.py',
        )
        _overprint(f'Updating {update_path}...')
        with open(update_path, 'r') as pkg_file:
            contents = '\n'.join(b2g.update_pw_package(name, pkg_file))
        with open(update_path, 'w') as pkg_file:
            pkg_file.write(contents)
        print(f'Updating {update_path}  with current revision.')

    for name in names:
        update_path = third_party_path.joinpath(name, 'docs.rst')
        _overprint(f'Updating {update_path}...')
        with open(update_path, 'r') as docs_rst:
            contents = '\n'.join(b2g.update_doc_rst(name, docs_rst))
        with open(update_path, 'w') as docs_rst:
            docs_rst.write(contents)
        print(f'Updated {update_path} with current revision.')

    print('Done!')

    print(
        'Make sure to update your WORKSPACE file to fetch the following '
        + 'revisions or later:'
    )
    for revision in b2g.get_workspace_revisions():
        print(revision)


if __name__ == '__main__':
    _bazel_to_gn(_parse_args())
