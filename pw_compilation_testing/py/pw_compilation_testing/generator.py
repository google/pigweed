# Copyright 2022 The Pigweed Authors
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
"""Generates compile fail test GN targets.

Scans source files for PW_NC_TEST(...) statements and generates a
BUILD.gn file with a target for each test. This allows the compilation failure
tests to run in parallel in Ninja.

This file is executed during gn gen, so it cannot rely on any setup that occurs
during the build.
"""

import argparse
import base64
from collections import defaultdict
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
import pickle
import re
import sys
from typing import (Iterable, Iterator, List, NamedTuple, NoReturn, Pattern,
                    Sequence, Set, Tuple)

# Matches the #if or #elif statement that starts a compile fail test.
_TEST_START = re.compile(br'^[ \t]*#[ \t]*(?:el)?if[ \t]+PW_NC_TEST\([ \t]*')

# Matches the name of a test case.
_TEST_NAME = re.compile(
    br'(?P<name>[a-zA-Z0-9_]+)[ \t]*\)[ \t]*(?://.*|/\*.*)?$')

# Negative compilation test commands take the form PW_NC_EXPECT("regex"),
# PW_NC_EXPECT_GCC("regex"), or PW_NC_EXPECT_CLANG("regex"). PW_NC_EXPECT() is
# an error.
_EXPECT_START = re.compile(br'^[ \t]*PW_NC_EXPECT(?P<compiler>_GCC|_CLANG)?\(')

# EXPECT statements are regular expressions that must match the compiler output.
# They must fit on a single line.
_EXPECT_REGEX = re.compile(br'(?P<regex>"[^\n]+")\);[ \t]*(?://.*|/\*.*)?$')


class Compiler(Enum):
    ANY = 0
    GCC = 1
    CLANG = 2

    @staticmethod
    def from_command(command: str) -> 'Compiler':
        if command.endswith(('clang', 'clang++')):
            return Compiler.CLANG

        if command.endswith(('gcc', 'g++')):
            return Compiler.GCC

        raise ValueError(
            f"Unrecognized compiler '{command}'; update the Compiler enum "
            f'in {Path(__file__).name} to account for this')

    def matches(self, other: 'Compiler') -> bool:
        return self is other or self is Compiler.ANY or other is Compiler.ANY


@dataclass(frozen=True)
class Expectation:
    compiler: Compiler
    pattern: Pattern[str]
    line: int


@dataclass(frozen=True)
class TestCase:
    suite: str
    case: str
    expectations: Tuple[Expectation, ...]
    source: Path
    line: int

    def name(self) -> str:
        return f'{self.suite}.{self.case}'

    def serialize(self) -> str:
        return base64.b64encode(pickle.dumps(self)).decode()

    @classmethod
    def deserialize(cls, serialized: str) -> 'Expectation':
        return pickle.loads(base64.b64decode(serialized))


class ParseError(Exception):
    """Failed to parse a PW_NC_TEST."""
    def __init__(self, message: str, file: Path, lines: Sequence[bytes],
                 error_lines: Sequence[int]) -> None:
        for i in error_lines:
            message += (
                f'\n{file.name}:{i + 1}: {lines[i].decode(errors="replace")}')
        super().__init__(message)


class _NegativeCompilationTestSource:
    def __init__(self, file: Path) -> None:
        self._file = file
        self._lines = self._file.read_bytes().splitlines()

        self._parsed_expectations: Set[int] = set()

    def _error(self, message: str, *error_lines: int) -> NoReturn:
        raise ParseError(message, self._file, self._lines, error_lines)

    def _parse_expectations(self, start: int) -> Iterator[Expectation]:
        for index in range(start, len(self._lines)):
            line = self._lines[index]

            expect_match = _EXPECT_START.match(line)

            if not expect_match:
                # Skip empty lines or lines with comments
                if not line or line.isspace() or line.lstrip().startswith(
                    (b'//', b'/*')):
                    continue
                break

            compiler_str = expect_match['compiler'] or b'ANY'
            compiler = Compiler[compiler_str.lstrip(b'_').decode()]

            self._parsed_expectations.add(index)

            regex_match = _EXPECT_REGEX.match(line, expect_match.end())
            if not regex_match:
                self._error(
                    'Failed to parse PW_EXPECT_NC() statement. PW_EXPECT_NC() '
                    'statements must fit on one line and contain a single, '
                    'non-empty string literal.', index)

            # Evaluate the string from the C++ source as a raw literal.
            re_string = eval(b'r' + regex_match['regex'])  # pylint: disable=eval-used
            if not isinstance(re_string, str):
                self._error('The regular expression must be a string!', index)

            try:
                yield Expectation(compiler, re.compile(re_string), index + 1)
            except re.error as error:
                self._error('Invalid regular expression: ' + error.msg, index)

            start = regex_match.end()

    def _check_for_stray_expectations(self) -> None:
        all_expectations = frozenset(i for i in range(len(self._lines))
                                     if _EXPECT_START.match(self._lines[i]))
        stray = all_expectations - self._parsed_expectations
        if stray:
            self._error(f'Found {len(stray)} stray PW_NC_EXPECT() commands!',
                        *sorted(stray))

    def parse(self, suite: str) -> Iterator[TestCase]:
        """Finds all negative compilation tests in this source file."""
        for index, line in enumerate(self._lines):
            case_match = _TEST_START.match(line)
            if not case_match:
                continue

            name_match = _TEST_NAME.match(line, case_match.end())
            if not name_match:
                self._error(
                    'Negative compilation test syntax error. '
                    'Expected test name, found '
                    f"'{line[case_match.end():].decode(errors='replace')}'",
                    index)

            expectations = tuple(self._parse_expectations(index + 1))
            yield TestCase(suite, name_match['name'].decode(), expectations,
                           self._file, index + 1)

        self._check_for_stray_expectations()


def enumerate_tests(suite: str, paths: Iterable[Path]) -> Iterator[TestCase]:
    """Parses PW_NC_TEST statements from a file."""
    for path in paths:
        yield from _NegativeCompilationTestSource(path).parse(suite)


class SourceFile(NamedTuple):
    gn_path: str
    file_path: Path


def generate_gn_target(base: str, source_list: str, test: TestCase,
                       all_tests: str) -> Iterator[str]:
    yield f'''\
pw_python_action("{test.name()}.negative_compilation_test") {{
  script = "$dir_pw_compilation_testing/py/pw_compilation_testing/runner.py"
  inputs = [{source_list}]
  args = [
    "--toolchain-ninja=$_toolchain_ninja",
    "--target-ninja=$_target_ninja",
    "--test-data={test.serialize()}",
    "--all-tests={all_tests}",
  ]
  deps = ["{base}"]
  python_deps = [
    "$dir_pw_cli/py",
    "$dir_pw_compilation_testing/py",
  ]
  stamp = true
}}
'''


def generate_gn_build(base: str, sources: Iterable[SourceFile],
                      tests: List[TestCase], all_tests: str) -> Iterator[str]:
    """Generates the BUILD.gn file with compilation failure test targets."""
    _, base_name = base.rsplit(':', 1)

    yield 'import("//build_overrides/pigweed.gni")'
    yield ''
    yield 'import("$dir_pw_build/python_action.gni")'
    yield ''
    yield ('_toolchain_ninja = '
           'rebase_path("$root_out_dir/toolchain.ninja", root_build_dir)')
    yield ('_target_ninja = '
           f'rebase_path(get_label_info("{base}", "target_out_dir") +'
           f'"/{base_name}.ninja", root_build_dir)')
    yield ''

    gn_source_list = ', '.join(f'"{gn_path}"' for gn_path, _ in sources)
    for test in tests:
        yield from generate_gn_target(base, gn_source_list, test, all_tests)


def _main(name: str, base: str, sources: Iterable[SourceFile],
          output: Path) -> int:
    print_stderr = lambda s: print(s, file=sys.stderr)

    try:
        tests = list(enumerate_tests(name, (s.file_path for s in sources)))
    except ParseError as error:
        print_stderr(f'ERROR: {error}')
        return 1

    if not tests:
        print_stderr(f'The test "{name}" has no negative compilation tests!')
        print_stderr('Add PW_NC_TEST() cases or remove this negative '
                     'compilation test')
        return 1

    tests_by_case = defaultdict(list)
    for test in tests:
        tests_by_case[test.case].append(test)

    duplicates = [tests for tests in tests_by_case.values() if len(tests) > 1]
    if duplicates:
        print_stderr('There are duplicate negative compilation test cases!')
        print_stderr('The following test cases appear more than once:')
        for tests in duplicates:
            print_stderr(f'\n    {tests[0].case} ({len(tests)} occurrences):')
            for test in tests:
                print_stderr(f'        {test.source.name}:{test.line}')
        return 1

    output.mkdir(parents=True, exist_ok=True)
    build_gn = output.joinpath('BUILD.gn')
    with build_gn.open('w') as fd:
        for line in generate_gn_build(base, sources, tests,
                                      output.joinpath('tests.txt').as_posix()):
            print(line, file=fd)

    with output.joinpath('tests.txt').open('w') as fd:
        for test in tests:
            print(test.case, file=fd)

    # Print the test case names to stdout for consumption by GN.
    for test in tests:
        print(test.case)

    return 0


def _parse_args() -> dict:
    """Parses command-line arguments."""
    def source_file(arg: str) -> SourceFile:
        gn_path, file_path = arg.split(';', 1)
        return SourceFile(gn_path, Path(file_path))

    parser = argparse.ArgumentParser(
        description='Emits an error when a facade has a null backend')
    parser.add_argument('--output', type=Path, help='Output directory')
    parser.add_argument('--name', help='Name of the NC test')
    parser.add_argument('--base', help='GN label for the base target to build')
    parser.add_argument('sources',
                        nargs='+',
                        type=source_file,
                        help='Source file with the no-compile tests')
    return vars(parser.parse_args())


if __name__ == '__main__':
    sys.exit(_main(**_parse_args()))
