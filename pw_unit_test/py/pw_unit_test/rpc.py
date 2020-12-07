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
"""Utilities for running unit tests over Pigweed RPC."""

import abc
from dataclasses import dataclass
import logging
from typing import Iterable

from pw_unit_test_proto import unit_test_pb2  # type: ignore

import pw_rpc.client

_LOG = logging.getLogger(__name__)


@dataclass(frozen=True)
class TestCase:
    suite_name: str
    test_name: str
    file_name: str

    def __str__(self) -> str:
        return f'{self.suite_name}.{self.test_name}'

    def __repr__(self) -> str:
        return f'TestCase({str(self)})'


@dataclass(frozen=True)
class TestExpectation:
    expression: str
    evaluated_expression: str
    line_number: int
    success: bool

    def __str__(self) -> str:
        return self.expression

    def __repr__(self) -> str:
        return f'TestExpectation({str(self)})'


class EventHandler(abc.ABC):
    @abc.abstractmethod
    def run_all_tests_start(self):
        """Called before all tests are run."""

    @abc.abstractmethod
    def run_all_tests_end(self, passed_tests: int, failed_tests: int):
        """Called after the test run is complete."""

    @abc.abstractmethod
    def test_case_start(self, test_case: TestCase):
        """Called when a new test case is started."""

    @abc.abstractmethod
    def test_case_end(self, test_case: TestCase,
                      result: unit_test_pb2.TestCaseResult):
        """Called when a test case completes with its overall result."""

    @abc.abstractmethod
    def test_case_disabled(self, test_case: TestCase):
        """Called when a disabled test case is encountered."""

    @abc.abstractmethod
    def test_case_expect(self, test_case: TestCase,
                         expectation: TestExpectation):
        """Called after each expect/assert statement within a test case."""


class LoggingEventHandler(EventHandler):
    """Event handler that logs test events using Google Test format."""
    def run_all_tests_start(self):
        _LOG.info('[==========] Running all tests.')

    def run_all_tests_end(self, passed_tests: int, failed_tests: int):
        _LOG.info('[==========] Done running all tests.')
        _LOG.info('[  PASSED  ] %d test(s).', passed_tests)
        if failed_tests:
            _LOG.info('[  FAILED  ] %d test(s).', failed_tests)

    def test_case_start(self, test_case: TestCase):
        _LOG.info('[ RUN      ] %s', test_case)

    def test_case_end(self, test_case: TestCase,
                      result: unit_test_pb2.TestCaseResult):
        if result == unit_test_pb2.TestCaseResult.SUCCESS:
            _LOG.info('[       OK ] %s', test_case)
        else:
            _LOG.info('[  FAILED  ] %s', test_case)

    def test_case_disabled(self, test_case: TestCase):
        _LOG.info('Skipping disabled test %s', test_case)

    def test_case_expect(self, test_case: TestCase,
                         expectation: TestExpectation):
        result = 'Success' if expectation.success else 'Failure'
        log = _LOG.info if expectation.success else _LOG.error
        log('%s:%d: %s', test_case.file_name, expectation.line_number, result)
        log('      Expected: %s', expectation.expression)
        log('        Actual: %s', expectation.evaluated_expression)


def run_tests(
    rpcs: pw_rpc.client.Services,
    report_passed_expectations: bool = False,
    event_handlers: Iterable[EventHandler] = (LoggingEventHandler(), )):
    """Runs unit tests on a device over Pigweed RPC.

    Calls each of the provided event handlers as test events occur.
    """
    unit_test_service = rpcs.pw.unit_test.UnitTest  # type: ignore[attr-defined]

    for response in unit_test_service.Run(
            report_passed_expectations=report_passed_expectations):
        if response.HasField('test_case_start'):
            raw_test_case = response.test_case_start
            current_test_case = TestCase(raw_test_case.suite_name,
                                         raw_test_case.test_name,
                                         raw_test_case.file_name)

        for event_handler in event_handlers:
            if response.HasField('test_run_start'):
                event_handler.run_all_tests_start()
            elif response.HasField('test_run_end'):
                event_handler.run_all_tests_end(response.test_run_end.passed,
                                                response.test_run_end.failed)
            elif response.HasField('test_case_start'):
                event_handler.test_case_start(current_test_case)
            elif response.HasField('test_case_end'):
                event_handler.test_case_end(current_test_case,
                                            response.test_case_end)
            elif response.HasField('test_case_disabled'):
                event_handler.test_case_disabled(current_test_case)
            elif response.HasField('test_case_expectation'):
                raw_expectation = response.test_case_expectation
                expectation = TestExpectation(
                    raw_expectation.expression,
                    raw_expectation.evaluated_expression,
                    raw_expectation.line_number,
                    raw_expectation.success,
                )
                event_handler.test_case_expect(current_test_case, expectation)
