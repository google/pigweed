#!/usr/bin/env python3
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
"""Tests for presubmit tools."""

import dataclasses
import json
import re
import unittest
import unittest.mock

from pw_presubmit import presubmit


class TestFileFilter(unittest.TestCase):
    """Test FileFilter class"""

    @dataclasses.dataclass
    class TestData:
        filter: presubmit.FileFilter
        value: str
        expected: bool

    test_scenarios = (
        TestData(presubmit.FileFilter(endswith=('bar', 'foo')), 'foo', True),
        TestData(presubmit.FileFilter(endswith=('bar', 'boo')), 'foo', False),
        TestData(
            presubmit.FileFilter(exclude=(re.compile('a/.+'),), name=('foo',)),
            '/a/b/c/foo',
            False,
        ),
        TestData(
            presubmit.FileFilter(exclude=(re.compile('x/.+'),), name=('foo',)),
            '/a/b/c/foo',
            True,
        ),
        TestData(
            presubmit.FileFilter(exclude=(re.compile('a+'), re.compile('b+'))),
            'cccc',
            True,
        ),
        TestData(presubmit.FileFilter(name=('foo',)), 'foo', True),
        TestData(presubmit.FileFilter(name=('foo',)), 'food', False),
        TestData(presubmit.FileFilter(name=(re.compile('foo'),)), 'foo', True),
        TestData(
            presubmit.FileFilter(name=(re.compile('foo'),)), 'food', False
        ),
        TestData(presubmit.FileFilter(name=(re.compile('fo+'),)), 'foo', True),
        TestData(presubmit.FileFilter(name=(re.compile('fo+'),)), 'fd', False),
        TestData(
            presubmit.FileFilter(suffix=('.exe',)), 'a/b.py/foo.exe', True
        ),
        TestData(
            presubmit.FileFilter(suffix=('.py',)), 'a/b.py/foo.exe', False
        ),
        TestData(
            presubmit.FileFilter(suffix=('.exe',)), 'a/b.py/foo.py.exe', True
        ),
        TestData(
            presubmit.FileFilter(suffix=('.py',)), 'a/b.py/foo.py.exe', False
        ),
        TestData(presubmit.FileFilter(suffix=('.a', '.b')), 'foo.b', True),
        TestData(presubmit.FileFilter(suffix=('.a', '.b')), 'foo.c', False),
    )

    def test_matches(self):
        for test_num, test_data in enumerate(self.test_scenarios):
            with self.subTest(i=test_num):
                self.assertEqual(
                    test_data.filter.matches(test_data.value),
                    test_data.expected,
                )


def _fake_function_1(_):
    """Fake presubmit function."""


def _fake_function_2(_):
    """Fake presubmit function."""


def _all_substeps(program):
    substeps = {}
    for step in program:
        # pylint: disable=protected-access
        for sub in step.substeps():
            substeps[sub.name or step.name] = sub._func
        # pylint: enable=protected-access
    return substeps


class ProgramsTest(unittest.TestCase):
    """Tests the presubmit Programs abstraction."""

    def setUp(self):
        self._programs = presubmit.Programs(
            first=[_fake_function_1, (), [(_fake_function_2,)]],
            second=[_fake_function_2],
        )

    def test_empty(self):
        self.assertEqual({}, presubmit.Programs())

    def test_access_present_members_first(self):
        self.assertEqual('first', self._programs['first'].name)
        self.assertEqual(
            ('_fake_function_1', '_fake_function_2'),
            tuple(x.name for x in self._programs['first']),
        )

        self.assertEqual(2, len(self._programs['first']))
        substeps = _all_substeps(
            self._programs['first']  # pylint: disable=protected-access
        ).values()
        self.assertEqual(2, len(substeps))
        self.assertEqual((_fake_function_1, _fake_function_2), tuple(substeps))

    def test_access_present_members_second(self):
        self.assertEqual('second', self._programs['second'].name)
        self.assertEqual(
            ('_fake_function_2',),
            tuple(x.name for x in self._programs['second']),
        )

        self.assertEqual(1, len(self._programs['second']))
        substeps = _all_substeps(
            self._programs['second']  # pylint: disable=protected-access
        ).values()
        self.assertEqual(1, len(substeps))
        self.assertEqual((_fake_function_2,), tuple(substeps))

    def test_access_missing_member(self):
        with self.assertRaises(KeyError):
            _ = self._programs['not_there']

    def test_all_steps(self):
        all_steps = self._programs.all_steps()
        self.assertEqual(len(all_steps), 2)
        all_substeps = _all_substeps(all_steps.values())
        self.assertEqual(len(all_substeps), 2)

        # pylint: disable=protected-access
        self.assertEqual(all_substeps['_fake_function_1'], _fake_function_1)
        self.assertEqual(all_substeps['_fake_function_2'], _fake_function_2)
        # pylint: enable=protected-access


def properties(**kwargs):
    return {
        'input': {
            'properties': {
                '$pigweed/pipeline': kwargs,
            },
        },
    }


class LuciContextTest(unittest.TestCase):
    """Tests the creation of a LuciContext object."""

    def _create(self, **kwargs):  # pylint: disable=no-self-use
        kwargs.setdefault('BUILDBUCKET_ID', '123456')
        kwargs.setdefault('BUILDBUCKET_NAME', 'project:bucket:builder')
        kwargs.setdefault('BUILD_NUMBER', '123')
        kwargs.setdefault('SWARMING_TASK_ID', '1a2b3c')
        final_kwargs = {k: v for k, v in kwargs.items() if v is not None}
        return presubmit.LuciContext.create_from_environment(final_kwargs)

    def test_missing_buildbucket_id(self):
        ctx = self._create(BUILDBUCKET_ID=None)
        self.assertEqual(None, ctx)

    def test_missing_buildbucket_name(self):
        ctx = self._create(BUILDBUCKET_NAME=None)
        self.assertEqual(None, ctx)

    def test_missing_build_number(self):
        ctx = self._create(BUILD_NUMBER=None)
        self.assertEqual(None, ctx)

    def test_missing_swarming_task_id(self):
        ctx = self._create(SWARMING_TASK_ID=None)
        self.assertEqual(None, ctx)

    def test_all_env_vars_present_no_bb(self):
        with unittest.mock.patch(
            'pw_presubmit.presubmit.subprocess.check_output',
            return_value=json.dumps({}),
        ):
            ctx = self._create()
        self.assertIsInstance(ctx, presubmit.LuciContext)

        self.assertEqual(123456, ctx.buildbucket_id)
        self.assertEqual(123, ctx.build_number)
        self.assertEqual('project', ctx.project)
        self.assertEqual('bucket', ctx.bucket)
        self.assertEqual('builder', ctx.builder)
        self.assertEqual('1a2b3c', ctx.swarming_task_id)
        self.assertFalse(ctx.pipeline)

    def test_all_env_vars_present_no_pipeline(self):
        props = properties(
            inside_a_pipeline=False,
            round=3,
            builds_from_previous_iteration=[1, 2],
        )

        with unittest.mock.patch(
            'pw_presubmit.presubmit.subprocess.check_output',
            return_value=json.dumps(props),
        ):
            ctx = self._create()
        self.assertIsInstance(ctx, presubmit.LuciContext)
        self.assertFalse(ctx.pipeline)

    def test_all_env_vars_present_pipeline(self):
        props = properties(
            inside_a_pipeline=True,
            round=3,
            builds_from_previous_iteration=[1, 2],
        )

        with unittest.mock.patch(
            'pw_presubmit.presubmit.subprocess.check_output',
            return_value=json.dumps(props),
        ):
            ctx = self._create()
        self.assertIsInstance(ctx, presubmit.LuciContext)
        self.assertTrue(ctx.pipeline)
        self.assertIsInstance(ctx.pipeline, presubmit.LuciPipeline)
        self.assertEqual(3, ctx.pipeline.round)
        self.assertEqual(
            [1, 2], list(ctx.pipeline.builds_from_previous_iteration)
        )


if __name__ == '__main__':
    unittest.main()
