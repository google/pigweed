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
"""Utilities for running unit tests over :ref:`module-pw_rpc`."""

import enum
import abc
from dataclasses import dataclass
import logging
from typing import Iterable

from pw_rpc.client import Services
from pw_rpc.callback_client import OptionalTimeout, UseDefault
from pw_unit_test_proto import unit_test_pb2

_LOG = logging.getLogger(__package__)


@dataclass(frozen=True)
class TestCase:
    suite_name: str
    test_name: str
    file_name: str

    def __str__(self) -> str:
        return f'{self.suite_name}.{self.test_name}'

    def __repr__(self) -> str:
        return f'TestCase({str(self)})'


def _test_case(raw_test_case: unit_test_pb2.TestCaseDescriptor) -> TestCase:
    return TestCase(
        raw_test_case.suite_name,
        raw_test_case.test_name,
        raw_test_case.file_name,
    )


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


class TestCaseResult(enum.IntEnum):
    SUCCESS = unit_test_pb2.TestCaseResult.SUCCESS
    FAILURE = unit_test_pb2.TestCaseResult.FAILURE
    SKIPPED = unit_test_pb2.TestCaseResult.SKIPPED


class EventHandler(abc.ABC):
    @abc.abstractmethod
    def run_all_tests_start(self) -> None:
        """Called before all tests are run."""

    @abc.abstractmethod
    def run_all_tests_end(self, passed_tests: int, failed_tests: int) -> None:
        """Called after the test run is complete."""

    @abc.abstractmethod
    def test_case_start(self, test_case: TestCase) -> None:
        """Called when a new test case is started."""

    @abc.abstractmethod
    def test_case_end(
        self, test_case: TestCase, result: TestCaseResult
    ) -> None:
        """Called when a test case completes with its overall result."""

    @abc.abstractmethod
    def test_case_disabled(self, test_case: TestCase) -> None:
        """Called when a disabled test case is encountered."""

    @abc.abstractmethod
    def test_case_expect(
        self, test_case: TestCase, expectation: TestExpectation
    ) -> None:
        """Called after each expect or assert statement within a test case."""


class LoggingEventHandler(EventHandler):
    """Event handler that logs test events using Google Test format."""

    def run_all_tests_start(self) -> None:
        _LOG.info('[==========] Running all tests.')

    def run_all_tests_end(self, passed_tests: int, failed_tests: int) -> None:
        _LOG.info('[==========] Done running all tests.')
        _LOG.info('[  PASSED  ] %d test(s).', passed_tests)
        if failed_tests:
            _LOG.info('[  FAILED  ] %d test(s).', failed_tests)

    def test_case_start(self, test_case: TestCase) -> None:
        _LOG.info('[ RUN      ] %s', test_case)

    def test_case_end(
        self, test_case: TestCase, result: TestCaseResult
    ) -> None:
        if result == TestCaseResult.SUCCESS:
            _LOG.info('[       OK ] %s', test_case)
        else:
            _LOG.info('[  FAILED  ] %s', test_case)

    def test_case_disabled(self, test_case: TestCase) -> None:
        _LOG.info('Skipping disabled test %s', test_case)

    def test_case_expect(
        self, test_case: TestCase, expectation: TestExpectation
    ) -> None:
        result = 'Success' if expectation.success else 'Failure'
        log = _LOG.info if expectation.success else _LOG.error
        log('%s:%d: %s', test_case.file_name, expectation.line_number, result)
        log('      Expected: %s', expectation.expression)
        log('        Actual: %s', expectation.evaluated_expression)


@dataclass(frozen=True)
class TestRecord:
    """Records test results."""

    passing_tests: tuple[TestCase, ...]
    failing_tests: tuple[TestCase, ...]
    disabled_tests: tuple[TestCase, ...]

    def all_tests_passed(self) -> bool:
        return not self.failing_tests

    def __bool__(self) -> bool:
        return self.all_tests_passed()


def run_tests(
    rpcs: Services,
    report_passed_expectations: bool = False,
    test_suites: Iterable[str] = (),
    event_handlers: Iterable[EventHandler] = (LoggingEventHandler(),),
    timeout_s: OptionalTimeout = UseDefault.VALUE,
) -> TestRecord:
    """Runs unit tests on a device over :ref:`module-pw_rpc`.

    Calls each of the provided event handlers as test events occur, and returns
    ``True`` if all tests pass.
    """
    unit_test_service = rpcs.pw.unit_test.UnitTest  # type: ignore[attr-defined]
    request = unit_test_service.Run.request(
        report_passed_expectations=report_passed_expectations,
        test_suite=test_suites,
    )
    call = unit_test_service.Run.invoke(request, timeout_s=timeout_s)
    test_responses = iter(call)

    # Read the first response, which must be a test_run_start message.
    try:
        first_response = next(test_responses)
    except StopIteration:
        _LOG.error(
            'The "test_run_start" message was dropped! UnitTest.Run '
            'concluded with %s.',
            call.status,
        )
        raise

    if not first_response.HasField('test_run_start'):
        raise ValueError(
            'Expected a "test_run_start" response from pw.unit_test.Run, '
            'but received a different message type. A response may have been '
            'dropped.'
        )

    for event_handler in event_handlers:
        event_handler.run_all_tests_start()

    passing_tests: list[TestCase] = []
    failing_tests: list[TestCase] = []
    disabled_tests: list[TestCase] = []

    for response in test_responses:
        if response.HasField('test_run_start'):
            for event_handler in event_handlers:
                event_handler.run_all_tests_start()
        elif response.HasField('test_run_end'):
            for event_handler in event_handlers:
                event_handler.run_all_tests_end(
                    response.test_run_end.passed, response.test_run_end.failed
                )
            assert len(passing_tests) == response.test_run_end.passed
            assert len(failing_tests) == response.test_run_end.failed
            test_record = TestRecord(
                passing_tests=tuple(passing_tests),
                failing_tests=tuple(failing_tests),
                disabled_tests=tuple(disabled_tests),
            )
        elif response.HasField('test_case_start'):
            raw_test_case = response.test_case_start
            current_test_case = _test_case(raw_test_case)
            for event_handler in event_handlers:
                event_handler.test_case_start(current_test_case)
        elif response.HasField('test_case_end'):
            result = TestCaseResult(response.test_case_end)
            for event_handler in event_handlers:
                event_handler.test_case_end(current_test_case, result)
            if result == TestCaseResult.SUCCESS:
                passing_tests.append(current_test_case)
            else:
                failing_tests.append(current_test_case)
        elif response.HasField('test_case_disabled'):
            raw_test_case = response.test_case_disabled
            current_test_case = _test_case(raw_test_case)
            for event_handler in event_handlers:
                event_handler.test_case_disabled(current_test_case)
            disabled_tests.append(current_test_case)
        elif response.HasField('test_case_expectation'):
            raw_expectation = response.test_case_expectation
            expectation = TestExpectation(
                raw_expectation.expression,
                raw_expectation.evaluated_expression,
                raw_expectation.line_number,
                raw_expectation.success,
            )
            for event_handler in event_handlers:
                event_handler.test_case_expect(current_test_case, expectation)
    return test_record
