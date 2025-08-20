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
"""Tests for the workflow validator."""

import unittest

from pw_build.workflows.build_driver import BuildDriver
from pw_build.workflows.private import validator
from pw_build.proto import workflows_pb2, build_driver_pb2


class NopBuildDriver(BuildDriver):
    def generate_action_sequence(
        self, job: build_driver_pb2.JobRequest
    ) -> build_driver_pb2.JobResponse:
        return build_driver_pb2.JobResponse()


class ValidatorTest(unittest.TestCase):
    """Tests for the Validator class."""

    def test_check_fragment_has_unique_name(self):
        """Test that duplicate names are detected."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        workflow_suite.builds.add(name='same-name')
        workflow_suite.tools.add(name='same-name')
        with self.assertRaises(validator.ValidationError):
            val = validator.Validator(workflow_suite, {})
            val.check_fragment_has_unique_name(workflow_suite.builds[0])

    def test_check_fragment_has_name(self):
        """Test that all fragments are named."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        workflow_suite.tools.add()
        with self.assertRaises(validator.ValidationError):
            val = validator.Validator(workflow_suite, {})
            val.check_fragment_has_name(workflow_suite.tools[0])

    def test_check_config_references_real_build_type(self):
        """Test that configs reference real build types."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        config = workflow_suite.configs.add(
            name='test-config', build_type='bazel'
        )
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_config_references_real_build_type(config)

    def test_check_build_references_real_config(self):
        """Test that builds reference real configs."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        build = workflow_suite.builds.add(
            name='test-build', use_config='missing-config'
        )
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_build_references_real_config(build)

    def test_check_build_has_config(self):
        """Test that builds have a config."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        build = workflow_suite.builds.add(name='test-build')
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_build_has_config(build)

    def test_check_build_has_targets(self):
        """Test that builds have targets."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        build = workflow_suite.builds.add(name='test-build')
        build.build_config.name = 'test-config'
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_build_has_targets(build)

    def test_check_tool_references_real_config(self):
        """Test that tools reference real configs."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        tool = workflow_suite.tools.add(
            name='test-tool', use_config='missing-config'
        )
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_tool_references_real_config(tool)

    def test_check_tool_has_config(self):
        """Test that tools have a config."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        tool = workflow_suite.tools.add(name='test-tool')
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_tool_has_config(tool)

    def test_check_tool_has_target(self):
        """Test that tools have a target."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        tool = workflow_suite.tools.add(name='test-tool')
        tool.build_config.name = 'test-config'
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_tool_has_target(tool)

    def test_check_group_builds_exist(self):
        """Test that group builds exist."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        group = workflow_suite.groups.add(name='test-group')
        group.builds.append('missing-build')
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_group_builds_exist(group)

    def test_check_group_analyzers_exist(self):
        """Test that group analyzers exist."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        group = workflow_suite.groups.add(name='test-group')
        group.analyzers.append('missing-analyzer')
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_group_analyzers_exist(group)

    def test_check_general_tools_do_not_set_rerun_shortcut(self):
        """Test that tools missing analyzer_safe_args do not set shortcut."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        general_tool = workflow_suite.tools.add(
            name='test-tool', rerun_shortcut='test-tool-alt'
        )
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_tool_rerun_shortcut_if_hybrid(general_tool)

    def test_check_analyzers_do_not_set_rerun_shortcut(self):
        """Test analyzers do not use rerun_shortcut."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        analyzer = workflow_suite.tools.add(
            name='test-tool',
            type=workflows_pb2.Tool.Type.ANALYZER,
            rerun_shortcut='test-tool-alt',
        )
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_tool_rerun_shortcut_if_hybrid(analyzer)

    def test_check_group_analyzers_are_tools(self):
        """Test that group analyzers are tools."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        group = workflow_suite.groups.add(name='test-group')
        group.analyzers.append('not-a-tool')
        workflow_suite.builds.add(name='not-a-tool')
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_group_analyzers_exist(group)

    def test_check_group_analyzers_are_analyzers(self):
        """Test that group analyzers are of type ANALYZER."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        group = workflow_suite.groups.add(name='test-group')
        group.analyzers.append('not-an-analyzer')
        tool = workflow_suite.tools.add(name='not-an-analyzer')
        tool.type = workflows_pb2.Tool.Type.GENERAL
        val = validator.Validator(workflow_suite, {})
        with self.assertRaises(validator.ValidationError):
            val.check_group_analyzers_exist(group)

    @staticmethod
    def test_validate_valid_config():
        """Test that a valid config passes validation."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        workflow_suite.configs.add(name='test-config', build_type='bazel')
        build = workflow_suite.builds.add(
            name='test-build', use_config='test-config'
        )
        build.targets.append('//:test')
        tool = workflow_suite.tools.add(
            name='test-tool', use_config='test-config'
        )
        tool.target = '//:test_tool'
        group = workflow_suite.groups.add(name='test-group')
        group.builds.append('test-build')
        group.analyzers.append('test-tool')
        tool.type = workflows_pb2.Tool.Type.ANALYZER
        val = validator.Validator(workflow_suite, {'bazel': NopBuildDriver()})
        val.validate()

    def test_validate_invalid_config(self):
        """Test that an invalid config fails validation."""
        workflow_suite = workflows_pb2.WorkflowSuite()
        workflow_suite.builds.add(name='same-name')
        workflow_suite.tools.add(name='same-name')
        with self.assertRaises(validator.ValidationError):
            val = validator.Validator(workflow_suite, {})
            val.validate()


if __name__ == '__main__':
    unittest.main()
