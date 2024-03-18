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
"""Tools for generating Pigweed tests that execute in C++ and Python."""

import argparse
from dataclasses import dataclass
from datetime import datetime
from collections import defaultdict
import unittest

from typing import (
    Any,
    Callable,
    Generic,
    Iterable,
    Iterator,
    Sequence,
    TextIO,
    TypeVar,
    Union,
)

_COPYRIGHT = f"""\
// Copyright {datetime.now().year} The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

// AUTOGENERATED - DO NOT EDIT
//
// Generated at {datetime.now().isoformat()}
"""

_HEADER_CPP = (
    _COPYRIGHT
    + """\
// clang-format off
"""
)

_HEADER_JS = (
    _COPYRIGHT
    + """\
/* eslint-env browser, jasmine */
"""
)


class Error(Exception):
    """Something went wrong when generating tests."""


T = TypeVar('T')


@dataclass
class Context(Generic[T]):
    """Info passed into test generator functions for each test case."""

    group: str
    count: int
    total: int
    test_case: T

    def cc_name(self) -> str:
        name = ''.join(
            w.capitalize() for w in self.group.replace('-', ' ').split(' ')
        )
        name = ''.join(c if c.isalnum() else '_' for c in name)
        return f'{name}_{self.count}' if self.total > 1 else name

    def py_name(self) -> str:
        name = 'test_' + ''.join(
            c if c.isalnum() else '_' for c in self.group.lower()
        )
        return f'{name}_{self.count}' if self.total > 1 else name

    def ts_name(self) -> str:
        name = ''.join(c if c.isalnum() else ' ' for c in self.group.lower())
        return f'{name} {self.count}' if self.total > 1 else name


# Test cases are specified as a sequence of strings or test case instances. The
# strings are used to separate the tests into named groups. For example:
#
#   STR_SPLIT_TEST_CASES = (
#     'Empty input',
#     MyTestCase('', '', []),
#     MyTestCase('', 'foo', []),
#     'Split on single character',
#     MyTestCase('abcde', 'c', ['ab', 'de']),
#     ...
#   )
#
GroupOrTest = Union[str, T]

# Python tests are generated by a function that returns a function usable as a
# unittest.TestCase method.
PyTest = Callable[[unittest.TestCase], None]
PyTestGenerator = Callable[[Context[T]], PyTest]

# C++ tests are generated with a function that returns or yields lines of C++
# code for the given test case.
CcTestGenerator = Callable[[Context[T]], Iterable[str]]

JsTestGenerator = Callable[[Context[T]], Iterable[str]]


class TestGenerator(Generic[T]):
    """Generates tests for multiple languages from a series of test cases."""

    def __init__(self, test_cases: Sequence[GroupOrTest[T]]):
        self._cases: dict[str, list[T]] = defaultdict(list)
        message = ''

        if len(test_cases) < 2:
            raise Error('At least one test case must be provided')

        if not isinstance(test_cases[0], str):
            raise Error(
                'The first item in the test cases must be a group name string'
            )

        for case in test_cases:
            if isinstance(case, str):
                message = case
            else:
                self._cases[message].append(case)

        if '' in self._cases:
            raise Error('Empty test group names are not permitted')

    def _test_contexts(self) -> Iterator[Context[T]]:
        for group, test_list in self._cases.items():
            for i, test_case in enumerate(test_list, 1):
                yield Context(group, i, len(test_list), test_case)

    def _generate_python_tests(self, define_py_test: PyTestGenerator):
        tests: dict[str, Callable[[Any], None]] = {}

        for ctx in self._test_contexts():
            test = define_py_test(ctx)
            test.__name__ = ctx.py_name()

            if test.__name__ in tests:
                raise Error(f'Multiple Python tests are named {test.__name__}!')

            tests[test.__name__] = test

        return tests

    def python_tests(self, name: str, define_py_test: PyTestGenerator) -> type:
        """Returns a Python unittest.TestCase class with tests for each case."""
        return type(
            name,
            (unittest.TestCase,),
            self._generate_python_tests(define_py_test),
        )

    def _generate_cc_tests(
        self, define_cpp_test: CcTestGenerator, header: str, footer: str
    ) -> Iterator[str]:
        yield _HEADER_CPP
        yield header

        for ctx in self._test_contexts():
            yield from define_cpp_test(ctx)
            yield ''

        yield footer

    def cc_tests(
        self,
        output: TextIO,
        define_cpp_test: CcTestGenerator,
        header: str,
        footer: str,
    ):
        """Writes C++ unit tests for each test case to the given file."""
        for line in self._generate_cc_tests(define_cpp_test, header, footer):
            output.write(line)
            output.write('\n')

    def _generate_ts_tests(
        self, define_ts_test: JsTestGenerator, header: str, footer: str
    ) -> Iterator[str]:
        yield _HEADER_JS
        yield header

        for ctx in self._test_contexts():
            yield from define_ts_test(ctx)
        yield footer

    def ts_tests(
        self,
        output: TextIO,
        define_js_test: JsTestGenerator,
        header: str,
        footer: str,
    ):
        """Writes JS unit tests for each test case to the given file."""
        for line in self._generate_ts_tests(define_js_test, header, footer):
            output.write(line)
            output.write('\n')


def _to_chars(data: bytes) -> Iterator[str]:
    for i, byte in enumerate(data):
        try:
            char = data[i : i + 1].decode()
            yield char if char.isprintable() else fr'\x{byte:02x}'
        except UnicodeDecodeError:
            yield fr'\x{byte:02x}'


def cc_string(data: Union[str, bytes]) -> str:
    """Returns a C++ string literal version of a byte string or UTF-8 string."""
    if isinstance(data, str):
        data = data.encode()

    return '"' + ''.join(_to_chars(data)) + '"'


def parse_test_generation_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Generate unit test files')
    parser.add_argument(
        '--generate-cc-test',
        type=argparse.FileType('w'),
        help='Generate the C++ test file',
    )
    parser.add_argument(
        '--generate-ts-test',
        type=argparse.FileType('w'),
        help='Generate the JS test file',
    )
    return parser.parse_known_args()[0]
