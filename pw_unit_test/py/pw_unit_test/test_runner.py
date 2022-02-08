# Copyright 2019 The Pigweed Authors
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
"""Runs Pigweed unit tests built using GN."""

import argparse
import asyncio
import enum
import json
import logging
import os
import subprocess
import sys

from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

import pw_cli.log
import pw_cli.process

# Global logger for the script.
_LOG: logging.Logger = logging.getLogger(__name__)


def register_arguments(parser: argparse.ArgumentParser) -> None:
    """Registers command-line arguments."""

    parser.add_argument('--root',
                        type=str,
                        default='out',
                        help='Path to the root build directory')
    parser.add_argument('-r',
                        '--runner',
                        type=str,
                        required=True,
                        help='Executable which runs a test on the target')
    parser.add_argument('-m',
                        '--timeout',
                        type=float,
                        help='Timeout for test runner in seconds')
    parser.add_argument('runner_args',
                        nargs="*",
                        help='Arguments to forward to the test runner')

    # The runner script can either run binaries directly or groups.
    group = parser.add_mutually_exclusive_group()
    group.add_argument('-g',
                       '--group',
                       action='append',
                       help='Test groups to run')
    group.add_argument('-t',
                       '--test',
                       action='append',
                       help='Test binaries to run')


class TestResult(enum.Enum):
    """Result of a single unit test run."""
    UNKNOWN = 0
    SUCCESS = 1
    FAILURE = 2


class Test:
    """A unit test executable."""
    def __init__(self, name: str, file_path: str):
        self.name: str = name
        self.file_path: str = file_path
        self.status: TestResult = TestResult.UNKNOWN

    def __repr__(self) -> str:
        return f'Test({self.name})'

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Test):
            return NotImplemented
        return self.file_path == other.file_path

    def __hash__(self) -> int:
        return hash(self.file_path)


class TestGroup:
    """Graph node representing a group of unit tests."""
    def __init__(self, name: str, tests: Iterable[Test]):
        self._name: str = name
        self._deps: Iterable['TestGroup'] = []
        self._tests: Iterable[Test] = tests

    def set_deps(self, deps: Iterable['TestGroup']) -> None:
        """Updates the dependency list of this group."""
        self._deps = deps

    def all_test_dependencies(self) -> List[Test]:
        """Returns a list of all tests in this group and its dependencies."""
        return list(self._all_test_dependencies(set()))

    def _all_test_dependencies(self, processed_groups: Set[str]) -> Set[Test]:
        if self._name in processed_groups:
            return set()

        tests: Set[Test] = set()
        for dep in self._deps:
            tests.update(
                dep._all_test_dependencies(  # pylint: disable=protected-access
                    processed_groups))

        tests.update(self._tests)
        processed_groups.add(self._name)

        return tests

    def __repr__(self) -> str:
        return f'TestGroup({self._name})'


class TestRunner:
    """Runs unit tests by calling out to a runner script."""
    def __init__(self,
                 executable: str,
                 args: Sequence[str],
                 tests: Iterable[Test],
                 timeout: Optional[float] = None):
        self._executable: str = executable
        self._args: Sequence[str] = args
        self._tests: List[Test] = list(tests)
        self._timeout = timeout

    async def run_tests(self) -> None:
        """Runs all registered unit tests through the runner script."""

        for idx, test in enumerate(self._tests, 1):
            total = str(len(self._tests))
            test_counter = f'Test {idx:{len(total)}}/{total}'

            _LOG.info('%s: [ RUN] %s', test_counter, test.name)

            # Convert POSIX to native directory seperators as GN produces '/'
            # but the Windows test runner needs '\\'.
            command = [
                str(Path(self._executable)),
                str(Path(test.file_path)), *self._args
            ]

            if self._executable.endswith('.py'):
                command.insert(0, sys.executable)

            try:
                process = await pw_cli.process.run_async(*command,
                                                         timeout=self._timeout)
                if process.returncode == 0:
                    test.status = TestResult.SUCCESS
                    test_result = 'PASS'
                else:
                    test.status = TestResult.FAILURE
                    test_result = 'FAIL'

                    _LOG.log(pw_cli.log.LOGLEVEL_STDOUT, '[%s]\n%s',
                             pw_cli.color.colors().bold_white(process.pid),
                             process.output.decode(errors='ignore').rstrip())

                    _LOG.info('%s: [%s] %s', test_counter, test_result,
                              test.name)
            except subprocess.CalledProcessError as err:
                _LOG.error(err)
                return

    def all_passed(self) -> bool:
        """Returns true if all unit tests passed."""
        return all(test.status is TestResult.SUCCESS for test in self._tests)


# Filename extension for unit test metadata files.
METADATA_EXTENSION = '.testinfo.json'


def find_test_metadata(root: str) -> List[str]:
    """Locates all test metadata files located within a directory tree."""

    metadata: List[str] = []
    for path, _, files in os.walk(root):
        for filename in files:
            if not filename.endswith(METADATA_EXTENSION):
                continue

            full_path = os.path.join(path, filename)
            _LOG.debug('Found group metadata at %s', full_path)
            metadata.append(full_path)

    return metadata


# TODO(frolv): This is copied from the Python runner script.
# It should be extracted into a library and imported instead.
def find_binary(target: str) -> str:
    """Tries to find a binary for a gn build target.

    Args:
        target: Relative filesystem path to the target's output directory and
            target name, separated by a colon.

    Returns:
        Full path to the target's binary.

    Raises:
        FileNotFoundError: No binary found for target.
    """

    target_path, target_name = target.split(':')

    for extension in ['', '.elf', '.exe']:
        potential_filename = f'{target_path}/{target_name}{extension}'
        if os.path.isfile(potential_filename):
            return potential_filename

    raise FileNotFoundError(
        f'Could not find output binary for build target {target}')


def parse_metadata(metadata: List[str], root: str) -> Dict[str, TestGroup]:
    """Builds a graph of test group objects from metadata.

    Args:
        metadata: List of paths to JSON test metadata files.
        root: Root output directory of the build.

    Returns:
        Map of group name to TestGroup object. All TestGroup objects are fully
        populated with the paths to their unit tests and references to their
        dependencies.
    """
    def canonicalize(path: str) -> str:
        """Removes a trailing slash from a GN target's directory.

        '//module:target'  -> '//module:target'
        '//module/:target' -> '//module:target'
        """
        index = path.find(':')
        if index == -1 or path[index - 1] != '/':
            return path
        return path[:index - 1] + path[index:]

    group_deps: List[Tuple[str, List[str]]] = []
    all_tests: Dict[str, Test] = {}
    test_groups: Dict[str, TestGroup] = {}
    num_tests = 0

    for path in metadata:
        with open(path, 'r') as metadata_file:
            metadata_list = json.load(metadata_file)

        deps: List[str] = []
        tests: List[Test] = []

        for entry in metadata_list:
            if entry['type'] == 'self':
                group_name = canonicalize(entry['name'])
            elif entry['type'] == 'dep':
                deps.append(canonicalize(entry['group']))
            elif entry['type'] == 'test':
                test_directory = os.path.join(root, entry['test_directory'])
                test_binary = find_binary(
                    f'{test_directory}:{entry["test_name"]}')

                if test_binary not in all_tests:
                    all_tests[test_binary] = Test(entry['test_name'],
                                                  test_binary)

                tests.append(all_tests[test_binary])

        if deps:
            group_deps.append((group_name, deps))

        num_tests += len(tests)
        test_groups[group_name] = TestGroup(group_name, tests)

    for name, deps in group_deps:
        test_groups[name].set_deps([test_groups[dep] for dep in deps])

    _LOG.info('Found %d test groups (%d tests).', len(metadata), num_tests)
    return test_groups


def tests_from_groups(group_names: Optional[Sequence[str]],
                      root: str) -> List[Test]:
    """Returns unit tests belonging to test groups and their dependencies.

    If args.names is nonempty, only searches groups specified there.
    Otherwise, finds tests from all known test groups.
    """

    _LOG.info('Scanning for tests...')
    metadata = find_test_metadata(root)
    test_groups = parse_metadata(metadata, root)

    groups_to_run = group_names if group_names else test_groups.keys()
    tests_to_run: Set[Test] = set()

    for name in groups_to_run:
        try:
            tests_to_run.update(test_groups[name].all_test_dependencies())
        except KeyError:
            _LOG.error('Unknown test group: %s', name)
            sys.exit(1)

    _LOG.info('Running test groups %s', ', '.join(groups_to_run))
    return list(tests_to_run)


def tests_from_paths(paths: Sequence[str]) -> List[Test]:
    """Returns a list of tests from test executable paths."""

    tests: List[Test] = []
    for path in paths:
        name = os.path.splitext(os.path.basename(path))[0]
        tests.append(Test(name, path))
    return tests


async def find_and_run_tests(
    root: str,
    runner: str,
    timeout: Optional[float],
    runner_args: Sequence[str] = (),
    group: Optional[Sequence[str]] = None,
    test: Optional[Sequence[str]] = None,
) -> int:
    """Runs some unit tests."""

    if test:
        tests = tests_from_paths(test)
    else:
        tests = tests_from_groups(group, root)

    test_runner = TestRunner(runner, runner_args, tests, timeout)
    await test_runner.run_tests()

    return 0 if test_runner.all_passed() else 1


def main() -> int:
    """Run Pigweed unit tests built using GN."""

    parser = argparse.ArgumentParser(description=main.__doc__)
    register_arguments(parser)
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Output additional logs as the script runs')

    args_as_dict = dict(vars(parser.parse_args()))
    del args_as_dict['verbose']
    return asyncio.run(find_and_run_tests(**args_as_dict))


if __name__ == '__main__':
    pw_cli.log.install(hide_timestamp=True)
    sys.exit(main())
