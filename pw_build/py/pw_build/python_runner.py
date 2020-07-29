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
"""Script that preprocesses a Python command then runs it.

This script evaluates expressions in the Python command's arguments then invokes
the command. Only one expression is supported currently:

  <TARGET_FILE(gn_target)> -- gets the target output file (e.g. .elf, .a,, .so)
      for a GN target; raises an error for targets with no output file, such as
      a source_set or group
"""

import argparse
from dataclasses import dataclass
import logging
from pathlib import Path
import re
import shlex
import subprocess
import sys
from typing import Callable, Dict, Iterator, List, NamedTuple, Optional, Tuple

_LOG = logging.getLogger(__name__)


def _parse_args() -> argparse.Namespace:
    """Parses arguments for this script, splitting out the command to run."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gn-root',
                        type=Path,
                        required=True,
                        help=('Path to the root of the GN tree; '
                              'value of rebase_path("//")'))
    parser.add_argument('--current-path',
                        type=Path,
                        required=True,
                        help='Value of rebase_path(".")')
    parser.add_argument('--default-toolchain',
                        required=True,
                        help='Value of default_toolchain')
    parser.add_argument('--current-toolchain',
                        required=True,
                        help='Value of current_toolchain')
    parser.add_argument(
        '--touch',
        type=Path,
        help='File to touch after the command is run',
    )
    parser.add_argument(
        '--capture-output',
        action='store_true',
        help='Capture subcommand output; display only on error',
    )
    parser.add_argument(
        'original_cmd',
        nargs=argparse.REMAINDER,
        help='Python script with arguments to run',
    )
    return parser.parse_args()


class GnPaths(NamedTuple):
    """The set of paths needed to resolve GN paths to filesystem paths."""
    root: Path
    build: Path
    cwd: Path

    # Toolchain label or '' if using the default toolchain
    toolchain: str

    def resolve(self, gn_path: str) -> Path:
        """Resolves a GN path to a filesystem path."""
        if gn_path.startswith('//'):
            return self.root.joinpath(gn_path[2:]).resolve()

        return self.cwd.joinpath(gn_path).resolve()

    def resolve_paths(self, gn_paths: str, sep: str = ';') -> str:
        """Resolves GN paths to filesystem paths in a delimited string."""
        return sep.join(
            str(self.resolve(path)) for path in gn_paths.split(sep))


@dataclass(frozen=True)
class Label:
    """Represents a GN label."""
    name: str
    dir: Path
    relative_dir: Path
    toolchain: Optional['Label']
    out_dir: Path
    gen_dir: Path

    def __init__(self, paths: GnPaths, label: str):
        # Use this lambda to set attributes on this frozen dataclass.
        set_attr = lambda attr, val: object.__setattr__(self, attr, val)

        # Handle explicitly-specified toolchains
        if label.endswith(')'):
            label, toolchain = label[:-1].rsplit('(', 1)
        else:
            # Prevent infinite recursion for toolchains
            toolchain = paths.toolchain if paths.toolchain != label else ''

        set_attr('toolchain', Label(paths, toolchain) if toolchain else None)

        # Split off the :target, if provided, or use the last part of the path.
        try:
            directory, name = label.rsplit(':', 1)
        except ValueError:
            directory, name = label, label.rsplit('/', 1)[-1]

        set_attr('name', name)

        # Resolve the directory to an absolute path
        set_attr('dir', paths.resolve(directory))
        set_attr('relative_dir', self.dir.relative_to(paths.root))

        set_attr(
            'out_dir',
            paths.build / self.toolchain_name() / 'obj' / self.relative_dir)
        set_attr(
            'gen_dir',
            paths.build / self.toolchain_name() / 'gen' / self.relative_dir)

    def gn_label(self) -> str:
        label = f'//{self.relative_dir.as_posix()}:{self.name}'
        return f'{label}({self.toolchain!r})' if self.toolchain else label

    def toolchain_name(self) -> str:
        return self.toolchain.name if self.toolchain else ''

    def __repr__(self) -> str:
        return self.gn_label()


class _Artifact(NamedTuple):
    path: Path
    variables: Dict[str, str]


_GN_NINJA_BUILD_STATEMENT = re.compile(r'^build (.+):[ \n]')


def _parse_build_artifacts(build_dir: Path, fd) -> Iterator[_Artifact]:
    """Partially parses the build statements in a Ninja file."""
    lines = iter(fd)

    def next_line():
        try:
            return next(lines)
        except StopIteration:
            return None

    # Serves as the parse state (only two states)
    artifact: Optional[_Artifact] = None

    line = next_line()

    while line is not None:
        if artifact:
            if line.startswith('  '):  # build variable statements are indented
                key, value = (a.strip() for a in line.split('=', 1))
                artifact.variables[key] = value
                line = next_line()
            else:
                yield artifact
                artifact = None
        else:
            match = _GN_NINJA_BUILD_STATEMENT.match(line)
            if match:
                artifact = _Artifact(build_dir / match.group(1), {})

            line = next_line()

    if artifact:
        yield artifact


def _search_target_ninja(ninja_file: Path, paths: GnPaths,
                         target: Label) -> Tuple[Optional[Path], List[Path]]:
    """Parses the main output file and object files from <target>.ninja."""

    artifact: Optional[Path] = None
    objects: List[Path] = []

    _LOG.debug('Parsing target Ninja file %s for %s', ninja_file, target)

    with ninja_file.open() as fd:
        for path, variables in _parse_build_artifacts(paths.build, fd):
            # GN uses .stamp files when there is no build artifact.
            if path.suffix == '.stamp':
                continue

            if variables:
                assert not artifact, f'Multiple artifacts for {target}!'
                artifact = path
            else:
                objects.append(path)

    return artifact, objects


def _search_toolchain_ninja(paths: GnPaths, target: Label) -> Optional[Path]:
    """Searches the toolchain.ninja file for <target>.stamp.

    Files created by an action appear in toolchain.ninja instead of in their own
    <target>.ninja. If the specified target has a single output file in
    toolchain.ninja, this function returns its path.
    """
    ninja_file = paths.build / target.toolchain_name() / 'toolchain.ninja'

    _LOG.debug('Searching toolchain Ninja file %s for %s', ninja_file, target)

    stamp_dir = target.out_dir.relative_to(paths.build).as_posix()
    stamp_tool = f'{target.toolchain_name()}_stamp'
    stamp_statement = f'build {stamp_dir}/{target.name}.stamp: {stamp_tool} '

    with ninja_file.open() as fd:
        for line in fd:
            if line.startswith(stamp_statement):
                output_files = line[len(stamp_statement):].strip().split()
                if len(output_files) == 1:
                    return paths.build / output_files[0]

                break

    return None


@dataclass(frozen=True)
class TargetInfo:
    """Provides information about a target parsed from a .ninja file."""

    label: Label
    artifact: Optional[Path]
    object_files: Tuple[Path]

    def __init__(self, paths: GnPaths, target: str):
        object.__setattr__(self, 'label', Label(paths, target))

        ninja = self.label.out_dir / f'{self.label.name}.ninja'
        if ninja.exists():
            artifact, objects = _search_target_ninja(ninja, paths, self.label)
        else:
            artifact = _search_toolchain_ninja(paths, self.label)
            objects = []

        object.__setattr__(self, 'artifact', artifact)
        object.__setattr__(self, 'object_files', tuple(objects))

    def __repr__(self) -> str:
        return repr(self.label)


class ExpressionError(Exception):
    """An error occurred while parsing an expression."""


def _target_output_file(paths: GnPaths, target_name: str) -> str:
    target = TargetInfo(paths, target_name)

    if not target.artifact:
        raise ExpressionError(f'Target {target} has no output file!')

    return str(target.artifact)


_FUNCTIONS: Dict['str', Callable[[GnPaths, str], str]] = {
    'TARGET_FILE': _target_output_file,
}

_START_EXPRESSION = re.compile(fr'<({"|".join(_FUNCTIONS)})\(')


def _expand_expressions(paths: GnPaths, string: str) -> Iterator[str]:
    pos = None

    for match in _START_EXPRESSION.finditer(string):
        yield string[pos:match.start()]

        pos = string.find(')>', match.end())
        if pos == -1:
            raise ExpressionError('Parse error: no terminating ")>" '
                                  f'was found for "{string[match.start():]}"')

        yield _FUNCTIONS[match.group(1)](paths, string[match.end():pos])

        pos += 2  # skip the terminating ')>'

    yield string[pos:]


def expand_expressions(paths: GnPaths, arg: str) -> str:
    """Expands <FUNCTION(...)> expressions."""
    return ''.join(_expand_expressions(paths, arg))


def main(
    gn_root: Path,
    current_path: Path,
    original_cmd: List[str],
    default_toolchain: str,
    current_toolchain: str,
    capture_output: bool,
    touch: Optional[Path],
) -> int:
    """Script entry point."""

    if not original_cmd or original_cmd[0] != '--':
        _LOG.error('%s requires a command to run', sys.argv[0])
        return 1

    # GN build scripts are executed from the root build directory.
    root_build_dir = Path.cwd().resolve()

    tool = current_toolchain if current_toolchain != default_toolchain else ''
    paths = GnPaths(root=gn_root.resolve(),
                    build=root_build_dir,
                    cwd=current_path.resolve(),
                    toolchain=tool)

    command = [sys.executable]
    try:
        command += (expand_expressions(paths, arg) for arg in original_cmd[1:])
    except ExpressionError as err:
        _LOG.error('%s: %s', sys.argv[0], err)
        return 1

    _LOG.debug('RUN %s', ' '.join(shlex.quote(arg) for arg in command))

    if capture_output:
        completed_process = subprocess.run(
            command,
            # Combine stdout and stderr so that error messages are correctly
            # interleaved with the rest of the output.
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    else:
        completed_process = subprocess.run(command)

    if completed_process.returncode != 0:
        _LOG.debug('Command failed; exit code: %d',
                   completed_process.returncode)
        # TODO(pwbug/34): Print a cross-platform pastable-in-shell command, to
        # help users track down what is happening when a command is broken.
        if capture_output:
            sys.stdout.buffer.write(completed_process.stdout)
    elif touch:
        # If a stamp file is provided and the command executed successfully,
        # touch the stamp file to indicate a successful run of the command.
        _LOG.debug('TOUCH %s', touch)
        touch.touch()

    return completed_process.returncode


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
