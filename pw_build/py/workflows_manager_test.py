# Copyright 2025 The Pigweed Authors
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
"""Tests for the WorkflowsManager."""

import unittest
from unittest.mock import patch

from pathlib import Path

from pw_build.build_recipe import BuildRecipe
from pw_build.proto import build_driver_pb2, workflows_pb2
from pw_build.workflows.manager import WorkflowsManager, expand_action
from pw_build.workflows.build_driver import BuildDriver


class FakeBuildDriver(BuildDriver):
    """A fake build driver for testing."""

    def generate_action_sequence(
        self, job: build_driver_pb2.JobRequest
    ) -> build_driver_pb2.JobResponse:
        job_response = build_driver_pb2.JobResponse()
        action = build_driver_pb2.Action(
            executable='fake_executable',
            args=['fake_arg'],
            run_from=build_driver_pb2.Action.InvocationLocation.PROJECT_ROOT,
        )
        job_response.actions.append(action)
        return job_response


class WorkflowsManagerTest(unittest.TestCase):
    """Tests for the WorkflowsManager class."""

    # pylint: disable=too-many-public-methods

    def setUp(self):
        """Set up a test environment."""
        self.project_root = Path('/test/project/root')
        self.working_dir = Path('/test/working/dir')
        self.base_out_dir = Path('/test/working/dir/out')
        self.workflow_suite = workflows_pb2.WorkflowSuite(
            configs=[
                workflows_pb2.BuildConfig(
                    name='shared_config',
                    build_type='fake_build_type',
                )
            ],
            tools=[
                workflows_pb2.Tool(
                    name='my_tool',
                    build_config=workflows_pb2.BuildConfig(
                        name='tool_config',
                        build_type='fake_build_type',
                    ),
                    target='//my_tool',
                ),
                workflows_pb2.Tool(
                    name='tool_with_shared_config',
                    use_config='shared_config',
                    target='//another_tool',
                ),
                workflows_pb2.Tool(
                    name='analyzer_tool',
                    type=workflows_pb2.Tool.Type.ANALYZER,
                    build_config=workflows_pb2.BuildConfig(
                        name='analyzer_config',
                        build_type='fake_build_type',
                    ),
                    target='//my_tool',
                ),
                workflows_pb2.Tool(
                    name='analyzer_friendly_tool',
                    analyzer_friendly_args=['--analyzer-mode'],
                    build_config=workflows_pb2.BuildConfig(
                        name='analyzer_friendly_config',
                        build_type='fake_build_type',
                    ),
                    target='//my_tool',
                ),
            ],
            builds=[
                workflows_pb2.Build(
                    name='my_build',
                    targets=['//:my_target'],
                    build_config=workflows_pb2.BuildConfig(
                        name='build_config',
                        build_type='fake_build_type',
                    ),
                ),
                workflows_pb2.Build(
                    name='build_with_shared_config',
                    targets=['//:my_target'],
                    use_config='shared_config',
                ),
                workflows_pb2.Build(
                    name='build_with_args',
                    targets=['//:my_target'],
                    build_config=workflows_pb2.BuildConfig(
                        name='build_config_with_args',
                        build_type='fake_build_type',
                        args=['--existing-arg'],
                    ),
                ),
            ],
            groups=[
                workflows_pb2.TaskGroup(
                    name='my_group',
                    builds=['my_build'],
                    analyzers=['analyzer_tool'],
                )
            ],
        )
        self.build_drivers = {'fake_build_type': FakeBuildDriver()}

    def test_init_success(self):
        """Test successful initialization of WorkflowsManager."""
        with patch('pw_build.workflows.manager.Validator') as mock_validator:
            manager = WorkflowsManager(
                self.workflow_suite,
                self.build_drivers,
                self.working_dir,
                self.base_out_dir,
                self.project_root,
            )
            self.assertIsNotNone(manager)
            mock_validator.assert_called_once_with(
                self.workflow_suite, self.build_drivers
            )
            mock_validator.return_value.validate.assert_called_once()

    @patch.dict('os.environ', {}, clear=True)
    def test_init_no_project_root_raises_error(self):
        """Test that ValueError is raised if project root cannot be found."""
        with self.assertRaises(ValueError):
            WorkflowsManager(
                self.workflow_suite,
                self.build_drivers,
                self.working_dir,
                self.base_out_dir,
            )

    def test_program_tool_success(self):
        """Test generating recipes for a tool."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        recipes = manager.program_tool('my_tool', ['--forwarded'])
        self.assertEqual(len(recipes), 1)
        recipe = recipes[0]
        self.assertIsInstance(recipe, BuildRecipe)
        self.assertEqual(recipe.title, 'check my_tool')
        self.assertEqual(len(recipe.steps), 1)
        step = recipe.steps[0]
        self.assertEqual(step.command, ['fake_executable', 'fake_arg'])

    def test_program_tool_not_a_tool_raises_error(self):
        """Test that TypeError is raised for non-tool fragments."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with self.assertRaises(TypeError):
            manager.program_tool('my_build', [])

    def test_program_tool_as_analyzer_success(self):
        """Test running a tool as an analyzer."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        recipes = manager.program_tool('analyzer_tool', [], as_analyzer=True)
        self.assertEqual(len(recipes), 1)

    def test_program_tool_as_analyzer_friendly_success(self):
        """Test running an analyzer-friendly tool as an analyzer."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with patch.object(
            manager, '_create_build_recipes', return_value=[]
        ) as mock_create:
            manager.program_tool(
                'analyzer_friendly_tool', ['--user-arg'], as_analyzer=True
            )
            mock_create.assert_called_once()
            self.assertEqual(
                mock_create.call_args[0][1], ['--analyzer-mode', '--user-arg']
            )

    def test_program_tool_as_analyzer_not_friendly_raises_error(self):
        """Test TypeError for non-analyzer-friendly tool with as_analyzer."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with self.assertRaises(TypeError):
            manager.program_tool('my_tool', [], as_analyzer=True)

    def test_program_build_success(self):
        """Test generating recipes for a build."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        recipes = manager.program_build('my_build')
        self.assertEqual(len(recipes), 1)
        recipe = recipes[0]
        self.assertIsInstance(recipe, BuildRecipe)
        self.assertEqual(recipe.title, 'build my_build')
        self.assertEqual(recipe.steps[0].targets, ['//:my_target'])

    def test_program_build_not_a_build_raises_error(self):
        """Test that TypeError is raised for non-build fragments."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with self.assertRaises(TypeError):
            manager.program_build('my_tool')

    def test_program_group_success(self):
        """Test generating recipes for a group."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        recipes = manager.program_group('my_group')
        self.assertEqual(len(recipes), 2)
        build_recipe = next(r for r in recipes if r.title.startswith('build '))
        tool_recipe = next(r for r in recipes if r.title.startswith('check '))
        self.assertIsNotNone(build_recipe)
        self.assertIsNotNone(tool_recipe)

    def test_program_group_not_a_group_raises_error(self):
        """Test that TypeError is raised for non-group fragments."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with self.assertRaises(TypeError):
            manager.program_group('build my_build')

    def test_expand_action_simple(self):
        """Test simple variable expansion."""
        action = build_driver_pb2.Action(args=['hello ${planet}'])
        env = {'planet': 'world'}
        expanded = expand_action(action, env)
        self.assertEqual(expanded, ['hello world'])

    def test_expand_action_list(self):
        """Test list variable expansion."""
        action = build_driver_pb2.Action(args=['${items}'])
        env = {'items': ['a', 'b', 'c']}
        expanded = expand_action(action, env)
        self.assertEqual(expanded, ['a', 'b', 'c'])

    def test_expand_action_list_multiple_identifiers_raises_error(self):
        """Test that ValueError is raised for invalid list expansion."""
        action = build_driver_pb2.Action(args=['${items} ${other}'])
        env = {'items': ['a', 'b', 'c'], 'other': 'd'}
        with self.assertRaises(ValueError):
            expand_action(action, env)

    def test_create_build_recipes_with_shared_config(self):
        """Test creating recipes with a shared configuration."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        recipes = manager.program_build('build_with_shared_config')
        self.assertEqual(len(recipes), 1)
        self.assertEqual(recipes[0].title, 'build build_with_shared_config')

    def test_program_by_name_group_success(self):
        """Test program_by_name with a group."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with patch.object(manager, 'program_group') as mock_program_group:
            manager.program_by_name('my_group')
            mock_program_group.assert_called_once_with('my_group')

    def test_program_by_name_build_success(self):
        """Test program_by_name with a build."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with patch.object(manager, 'program_build') as mock_program_build:
            manager.program_by_name('my_build')
            mock_program_build.assert_called_once_with('my_build')

    def test_program_by_name_tool_success(self):
        """Test program_by_name with a tool."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with patch.object(manager, 'program_tool') as mock_program_tool:
            manager.program_by_name('my_tool')
            mock_program_tool.assert_called_once_with(
                'my_tool', forwarded_arguments=(), as_analyzer=True
            )

    def test_program_by_name_not_found_raises_error(self):
        """Test program_by_name with a non-existent name."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with self.assertRaises(TypeError):
            manager.program_by_name('not_a_real_program')

    def test_get_unified_driver_request_single_build(self):
        """Test get_unified_driver_request with a single build."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        request = manager.get_unified_driver_request(['my_build'])
        self.assertEqual(len(request.jobs), 1)
        self.assertEqual(request.jobs[0].build.name, 'my_build')
        self.assertEqual(
            request.jobs[0].build.build_config.name, 'build_config'
        )

    def test_get_unified_driver_request_single_build_sanitized(self):
        """Test get_unified_driver_request with a sanitized single build."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        request = manager.get_unified_driver_request(
            ['my_build'], sanitize=True
        )
        self.assertEqual(len(request.jobs), 1)
        self.assertEqual(request.jobs[0].build.name, '')
        self.assertEqual(request.jobs[0].build.build_config.name, '')

    def test_get_unified_driver_request_single_tool(self):
        """Test get_unified_driver_request with a single tool."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        request = manager.get_unified_driver_request(['my_tool'])
        self.assertEqual(len(request.jobs), 1)
        self.assertEqual(request.jobs[0].tool.name, 'my_tool')

    def test_get_unified_driver_request_group(self):
        """Test get_unified_driver_request with a group."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        request = manager.get_unified_driver_request(['my_group'])
        self.assertEqual(len(request.jobs), 2)
        job_fragments = [
            job.build if job.WhichOneof('type') == 'build' else job.tool
            for job in request.jobs
        ]
        job_names = {fragment.name for fragment in job_fragments}
        self.assertEqual(job_names, {'my_build', 'analyzer_tool'})

    def test_get_unified_driver_request_mix_build_and_tool(self):
        """Test get_unified_driver_request with a mix of build and tool."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        request = manager.get_unified_driver_request(['my_build', 'my_tool'])
        self.assertEqual(len(request.jobs), 2)
        job_fragments = [
            job.build if job.WhichOneof('type') == 'build' else job.tool
            for job in request.jobs
        ]
        job_names = {fragment.name for fragment in job_fragments}
        self.assertEqual(job_names, {'my_build', 'my_tool'})

    def test_get_unified_driver_request_non_existent_raises_error(self):
        """Test get_unified_driver_request with a non-existent name."""
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
        )
        with self.assertRaises(AssertionError):
            manager.get_unified_driver_request(['not_a_real_thing'])

    def test_extra_args_injected_into_build_without_args(self):
        """Test extra args injection into a build with no existing args."""
        extra_args = {'fake_build_type': ['--extra', '--args']}
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
            extra_build_args=extra_args,
        )
        driver = self.build_drivers['fake_build_type']
        with patch.object(
            driver, 'generate_jobs', wraps=driver.generate_jobs
        ) as mock_generate_jobs:
            manager.program_build('my_build')
            mock_generate_jobs.assert_called_once()
            request = mock_generate_jobs.call_args[0][0]
            self.assertEqual(len(request.jobs), 1)
            self.assertEqual(
                list(request.jobs[0].build.build_config.args),
                ['--extra', '--args'],
            )

    def test_extra_args_injected_into_build_with_args(self):
        """Test extra args injection into a build with existing args."""
        extra_args = {'fake_build_type': ['--extra', '--args']}
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
            extra_build_args=extra_args,
        )
        driver = self.build_drivers['fake_build_type']
        with patch.object(
            driver, 'generate_jobs', wraps=driver.generate_jobs
        ) as mock_generate_jobs:
            manager.program_build('build_with_args')
            mock_generate_jobs.assert_called_once()
            request = mock_generate_jobs.call_args[0][0]
            self.assertEqual(len(request.jobs), 1)
            self.assertEqual(
                list(request.jobs[0].build.build_config.args),
                ['--existing-arg', '--extra', '--args'],
            )

    def test_extra_args_injected_into_tool(self):
        """Test that extra args are correctly added to tools."""
        extra_args = {'fake_build_type': ['--extra', '--args']}
        manager = WorkflowsManager(
            self.workflow_suite,
            self.build_drivers,
            self.working_dir,
            self.base_out_dir,
            self.project_root,
            extra_build_args=extra_args,
        )
        driver = self.build_drivers['fake_build_type']
        with patch.object(
            driver, 'generate_jobs', wraps=driver.generate_jobs
        ) as mock_generate_jobs:
            manager.program_tool('my_tool', [])
            mock_generate_jobs.assert_called_once()
            request = mock_generate_jobs.call_args[0][0]
            self.assertEqual(len(request.jobs), 1)
            self.assertEqual(
                list(request.jobs[0].tool.build_config.args),
                ['--extra', '--args'],
            )


if __name__ == '__main__':
    unittest.main()
