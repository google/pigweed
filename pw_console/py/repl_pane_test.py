# Copyright 2021 The Pigweed Authors
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
"""Tests for pw_console.console_app"""

import asyncio
import builtins
import platform
import threading
import unittest
from inspect import cleandoc
from unittest.mock import Mock, MagicMock

from prompt_toolkit.application import create_app_session
# inclusive-language: ignore
from prompt_toolkit.output import DummyOutput as FakeOutput

from pw_console.console_app import ConsoleApp
from pw_console.repl_pane import ReplPane
from pw_console.pw_ptpython_repl import PwPtPythonRepl


class TestReplPane(unittest.TestCase):
    """Tests for ReplPane."""
    def test_repl_code_return_values(self) -> None:
        """Test stdout, return values, and exceptions can be returned from
        running user repl code."""
        # TODO(tonymd): Find out why this fails on windows.
        if platform.system() in ['Windows']:
            return
        app = Mock()

        global_vars = {
            '__name__': '__main__',
            '__package__': None,
            '__doc__': None,
            '__builtins__': builtins,
        }

        pw_ptpython_repl = PwPtPythonRepl(
            get_globals=lambda: global_vars,
            get_locals=lambda: global_vars,
        )
        repl_pane = ReplPane(
            application=app,
            python_repl=pw_ptpython_repl,
        )
        # Check pw_ptpython_repl has a reference to the parent repl_pane.
        self.assertEqual(repl_pane, pw_ptpython_repl.repl_pane)

        # Define a function, should return nothing.
        code = cleandoc("""
            def run():
                print('The answer is ', end='')
                return 1+1+4+16+20
        """)
        # pylint: disable=protected-access
        result = asyncio.run(pw_ptpython_repl._run_user_code(code))
        self.assertEqual(result, {'stdout': '', 'stderr': '', 'result': None})

        # Check stdout and return value
        result = asyncio.run(pw_ptpython_repl._run_user_code('run()'))
        self.assertEqual(result, {
            'stdout': 'The answer is ',
            'stderr': '',
            'result': 42
        })

        # Check for repl exception
        result = asyncio.run(pw_ptpython_repl._run_user_code('return "blah"'))
        self.assertIn("SyntaxError: 'return' outside function",
                      pw_ptpython_repl._last_result)

    def test_user_thread(self) -> None:
        """Test user code thread."""
        # TODO(tonymd): Find out why create_app_session isn't working here on
        # windows.
        if platform.system() in ['Windows']:
            return
        with create_app_session(output=FakeOutput()):
            app = ConsoleApp()
            app.start_user_code_thread()

            pw_ptpython_repl = app.pw_ptpython_repl
            repl_pane = app.repl_pane

            pw_ptpython_repl.user_code_complete_callback = MagicMock(
                wraps=pw_ptpython_repl.user_code_complete_callback)
            user_code_done = threading.Event()

            code = cleandoc("""
                import time
                def run():
                    time.sleep(0.3)
                    print('The answer is ', end='')
                    return 1+1+4+16+20
            """)

            input_buffer = MagicMock(text=code)
            # pylint: disable=protected-access
            pw_ptpython_repl._accept_handler(input_buffer)

            # Get last executed code object.
            user_code1 = repl_pane.executed_code[-1]
            # Wait for repl code to finish.
            user_code1.future.add_done_callback(
                lambda future: user_code_done.set())
            user_code_done.wait(timeout=3)

            pw_ptpython_repl.user_code_complete_callback.assert_called_once()
            self.assertIsNotNone(user_code1)
            self.assertTrue(user_code1.future.done())
            self.assertEqual(user_code1.input, code)
            self.assertEqual(user_code1.output, None)
            # stdout / stderr may be '' or None
            self.assertFalse(user_code1.stdout)
            self.assertFalse(user_code1.stderr)

            user_code_done.clear()
            pw_ptpython_repl.user_code_complete_callback.reset_mock()

            input_buffer = MagicMock(text='run()')
            pw_ptpython_repl._accept_handler(input_buffer)

            # Get last executed code object.
            user_code2 = repl_pane.executed_code[-1]
            # Wait for repl code to finish.
            user_code2.future.add_done_callback(
                lambda future: user_code_done.set())
            user_code_done.wait(timeout=3)

            pw_ptpython_repl.user_code_complete_callback.assert_called_once()
            self.assertIsNotNone(user_code2)
            self.assertTrue(user_code2.future.done())
            self.assertEqual(user_code2.input, 'run()')
            self.assertEqual(user_code2.output, '42')
            self.assertEqual(user_code2.stdout, 'The answer is ')
            self.assertFalse(user_code2.stderr)


if __name__ == '__main__':
    unittest.main()
