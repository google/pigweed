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
"""Functions for building code during presubmit checks."""

import collections
import logging
import os
from pathlib import Path
import re
from typing import Container, Dict, Iterable, List, Mapping, Set, Tuple

from pw_presubmit import call, log_run, plural, PresubmitFailure, tools

_LOG = logging.getLogger(__name__)


def gn_args(**kwargs) -> str:
    """Builds a string to use for the --args argument to gn gen."""
    return '--args=' + ' '.join(f'{arg}={val}' for arg, val in kwargs.items())


def gn_gen(gn_source_dir: Path,
           gn_output_dir: Path,
           *args: str,
           gn_check: bool = True,
           **gn_arguments) -> None:
    """Runs gn gen in the specified directory with optional GN args."""
    args_option = (gn_args(**gn_arguments), ) if gn_arguments else ()
    check_option = ['--check'] if gn_check else []

    call('gn',
         'gen',
         gn_output_dir,
         '--color=always',
         *check_option,
         *args,
         *args_option,
         cwd=gn_source_dir)


def ninja(directory: Path, *args, **kwargs) -> None:
    """Runs ninja in the specified directory."""
    call('ninja', '-C', directory, *args, **kwargs)


def cmake(source_dir: Path,
          output_dir: Path,
          env: Mapping['str', 'str'] = None) -> None:
    """Runs CMake for Ninja on the given source and output directories."""
    call('cmake', '-B', output_dir, '-S', source_dir, '-G', 'Ninja', env=env)


def env_with_clang_vars() -> Mapping[str, str]:
    """Returns the environment variables with CC, CXX, etc. set for clang."""
    env = os.environ.copy()
    env['CC'] = env['LD'] = env['AS'] = 'clang'
    env['CXX'] = 'clang++'
    return env


def _get_paths_from_command(source_dir: Path, *args, **kwargs) -> Set[Path]:
    """Runs a command and reads Bazel or GN //-style paths from it."""
    process = log_run(args, capture_output=True, cwd=source_dir, **kwargs)

    if process.returncode:
        _LOG.error('Build invocation failed with return code %d!',
                   process.returncode)
        _LOG.error('[COMMAND] %s\n%s\n%s', *tools.format_command(args, kwargs),
                   process.stderr.decode())
        raise PresubmitFailure

    files = set()

    for line in process.stdout.splitlines():
        path = line.strip().lstrip(b'/').replace(b':', b'/').decode()
        path = source_dir.joinpath(path)
        if path.is_file():
            files.add(path)

    return files


# Finds string literals with '.' in them.
_MAYBE_A_PATH = re.compile(r'"([^\n"]+\.[^\n"]+)"')


def _search_files_for_paths(build_files: Iterable[Path]) -> Iterable[Path]:
    for build_file in build_files:
        directory = build_file.parent

        for string in _MAYBE_A_PATH.finditer(build_file.read_text()):
            path = directory / string.group(1)
            if path.is_file():
                yield path


def check_builds_for_files(
        extensions_to_check: Container[str],
        files: Iterable[Path],
        bazel_dirs: Iterable[Path] = (),
        gn_dirs: Iterable[Tuple[Path, Path]] = (),
        gn_build_files: Iterable[Path] = (),
) -> Dict[str, List[Path]]:
    """Checks that source files are in the GN and Bazel builds.

    Args:
        extensions_to_check: which file suffixes to look for
        files: the files that should be checked
        bazel_dirs: directories in which to run bazel query
        gn_dirs: (source_dir, output_dir) tuples with which to run gn desc
        gn_build_files: paths to BUILD.gn files to directly search for paths

    Returns:
        a dictionary mapping build system ('Bazel' or 'GN' to a list of missing
        files; will be empty if there were no missing files
    """

    # Collect all paths in the Bazel builds.
    bazel_builds: Set[Path] = set()
    for directory in bazel_dirs:
        bazel_builds.update(
            _get_paths_from_command(directory, 'bazel', 'query',
                                    'kind("source file", //...:*)'))

    # Collect all paths in GN builds.
    gn_builds: Set[Path] = set()

    for source_dir, output_dir in gn_dirs:
        gn_builds.update(
            _get_paths_from_command(source_dir, 'gn', 'desc', output_dir, '*'))

    gn_builds.update(_search_files_for_paths(gn_build_files))

    missing: Dict[str, List[Path]] = collections.defaultdict(list)

    for path in (p for p in files if p.suffix in extensions_to_check):
        if bazel_dirs and path.suffix != '.rst' and path not in bazel_builds:
            # TODO(pwbug/176) Replace this workaround for fuzzers.
            if 'fuzz' not in str(path):
                missing['Bazel'].append(path)
        if (gn_dirs or gn_build_files) and path not in gn_builds:
            missing['GN'].append(path)

    for builder, paths in missing.items():
        _LOG.warning('%s missing from the %s build:\n%s',
                     plural(paths, 'file', are=True), builder,
                     '\n'.join(str(x) for x in paths))

    return missing
