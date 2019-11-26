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

import argparse
import logging
import pathlib
import glob
from enum import Enum
from typing import Callable
from typing import NamedTuple
_LOG = logging.getLogger(__name__)

CheckerFunction = Callable[[str], None]


def register_arguments(parser):
    parser.add_argument('module', help='The module to check')


def main(module):
    if not pathlib.Path(module).is_dir():
        _LOG.error('No directory found: %s', module)
        return 1

    found_any_warnings = False
    found_any_errors = False

    _LOG.info('Checking module: %s', module)
    # Run each checker.
    for checker in _checkers:
        _LOG.debug(
            'Running checker: %s - %s',
            checker.name,
            checker.description,
        )
        issues = list(checker.run(module))

        # Log any issues found
        for issue in issues:
            if issue.severity == Severity.ERROR:
                log_level = logging.ERROR
                found_any_errors = True
            elif issue.severity == Severity.WARNING:
                log_level = logging.WARNING
                found_any_warnings = True

            # Try to make an error message that will help editors open the part
            # of the module in question (e.g. vim's 'cerr' functionality).
            components = [
                x for x in (
                    issue.file,
                    issue.line_number,
                    issue.line_contents,
                ) if x
            ]
            editor_error_line = ':'.join(components)
            if editor_error_line:
                _LOG.log(log_level, '%s', checker.name)
                print(editor_error_line, issue.message)
            else:
                # No per-file error to put in a "cerr" list, so just log.
                _LOG.log(log_level, '%s: %s', checker.name, issue.message)

        if issues:
            _LOG.debug('Done running checker: %s (issues found)', checker.name)
        else:
            _LOG.debug('Done running checker: %s (OK)', checker.name)

    # TODO(keir): Give this a proper ASCII art treatment.
    if not found_any_warnings and not found_any_errors:
        _LOG.info('OK: Module %s looks good; no errors or warnings found',
                  module)
    if found_any_errors:
        _LOG.error('FAIL: Found errors when checking module %s', module)
        return 1


class Checker(NamedTuple):
    name: str
    description: str
    run: CheckerFunction


class Severity(Enum):
    ERROR = 1
    WARNING = 2


class Issue(NamedTuple):
    message: str
    file: str = ''
    line_number: str = ''
    line_contents: str = ''
    severity: Severity = Severity.ERROR


_checkers = []


def checker(pwck_id, description):
    def inner_decorator(function):
        _checkers.append(Checker(pwck_id, description, function))
        return function

    return inner_decorator


@checker('PWCK001', 'If there is Python code, there is a setup.py')
def check_python_proper_module(directory):
    module_python_files = glob.glob(f'{directory}/**/*.py', recursive=True)
    module_setup_py = glob.glob(f'{directory}/**/setup.py', recursive=True)
    if module_python_files and not module_setup_py:
        yield Issue('Python code present but no setup.py.')


@checker('PWCK002', 'If there are C++ files, there are C++ tests')
def check_have_cc_tests(directory):
    module_cc_files = glob.glob(f'{directory}/**/*.cc', recursive=True)
    module_cc_test_files = glob.glob(f'{directory}/**/*_test.cc',
                                     recursive=True)
    if module_cc_files and not module_cc_test_files:
        yield Issue('C++ code present but no tests at all (you monster).')


@checker('PWCK003', 'If there are Python files, there are Python tests')
def check_have_python_tests(directory):
    module_py_files = glob.glob(f'{directory}/**/*.py', recursive=True)
    module_py_test_files = glob.glob(f'{directory}/**/*test*.py',
                                     recursive=True)
    if module_py_files and not module_py_test_files:
        yield Issue('Python code present but no tests (you monster).')


# Register the checker plugin with the 'pw' command.
try:
    import pw_cli.plugins
    pw_cli.plugins.register(
        name='module-check',
        help="Check that a module matches Pigweed's module guidelines",
        command_function=main,
        define_args_function=register_arguments,
    )
except ImportError:
    pass
