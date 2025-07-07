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
"""Validation logic for Workflows configurations.

Any rules, preconditions, or relationships that are not strictly
enforced by the Workflows proto schema must be validated here.

Some high-level examples:

* `name` fields in the Workflows configurations are used as identifiers,
  and therefore must be unique.
* To prevent duplication and make authoring configurations, many components
  in a Workflows configuration may reference others by `name`. Since these are
  strings, checks must be done to ensure the referenced pieces both exist and
  are of the expected type.

Some pieces of the Workflows tooling are guarded by this validation logic to
unify preconditions. This, for example, allows all pieces of the Workflows
libraries to assume every referenceable piece of a configuration has a name that
is both populated an unique, which greatly cuts down on code duplication and
inline `if` statements.
"""

from collections.abc import Callable

from google.protobuf import text_format
from pw_build.proto import workflows_pb2
from pw_build.workflows.build_driver import BuildDriver


class ValidationError(Exception):
    """A Workflows configuration error."""


Fragment = (
    workflows_pb2.BuildConfig
    | workflows_pb2.TaskGroup
    | workflows_pb2.Tool
    | workflows_pb2.Build
)


def collect_all_fragments(
    workflow_suite: workflows_pb2.WorkflowSuite,
) -> list[Fragment]:
    all_fragments: list[Fragment] = []
    all_fragments.extend(workflow_suite.configs)
    all_fragments.extend(workflow_suite.builds)
    all_fragments.extend(
        [
            build.build_config
            for build in workflow_suite.builds
            if build.WhichOneof('config') == 'build_config'
        ]
    )
    all_fragments.extend(workflow_suite.tools)
    all_fragments.extend(
        [
            tool.build_config
            for tool in workflow_suite.tools
            if tool.WhichOneof('config') == 'build_config'
        ]
    )
    all_fragments.extend(workflow_suite.groups)
    return all_fragments


class Validator:
    """A class for validating workflows configurations."""

    def __init__(
        self,
        workflow_suite: workflows_pb2.WorkflowSuite,
        build_drivers: dict[str, BuildDriver],
    ):
        self._workflow_suite = workflow_suite
        self._build_drivers = build_drivers
        self._fragments_by_name: dict[str, Fragment] = {}
        self._all_fragments = collect_all_fragments(self._workflow_suite)
        self._fragments_by_name = {
            fragment.name: fragment for fragment in self._all_fragments
        }
        self._shared_config_names = {
            config.name for config in self._workflow_suite.configs
        }

    def validate(self) -> None:
        """Runs all checks on the loaded Workflows config.

        This runs all check_*() methods on this class, efficiently
        grouping by categories.

        """
        all_fragment_checks: list[Callable[[Fragment], None]] = []
        config_fragment_checks: list[
            Callable[[workflows_pb2.BuildConfig], None]
        ] = []
        group_fragment_checks: list[
            Callable[[workflows_pb2.TaskGroup], None]
        ] = []
        tool_fragment_checks: list[Callable[[workflows_pb2.Tool], None]] = []
        build_fragment_checks: list[Callable[[workflows_pb2.Build], None]] = []
        for attr in dir(self):
            if attr.startswith('check_fragment_'):
                all_fragment_checks.append(getattr(self, attr))
            elif attr.startswith('check_config_'):
                config_fragment_checks.append(getattr(self, attr))
            elif attr.startswith('check_build_'):
                build_fragment_checks.append(getattr(self, attr))
            elif attr.startswith('check_tool_'):
                tool_fragment_checks.append(getattr(self, attr))
            elif attr.startswith('check_group_'):
                group_fragment_checks.append(getattr(self, attr))
        for fragment in self._all_fragments:
            for check in all_fragment_checks:
                check(fragment)
            if isinstance(fragment, workflows_pb2.Build):
                for build_check in build_fragment_checks:
                    build_check(fragment)
            elif isinstance(fragment, workflows_pb2.BuildConfig):
                for config_check in config_fragment_checks:
                    config_check(fragment)
            elif isinstance(fragment, workflows_pb2.Tool):
                for tool_check in tool_fragment_checks:
                    tool_check(fragment)
            elif isinstance(fragment, workflows_pb2.TaskGroup):
                for group_check in group_fragment_checks:
                    group_check(fragment)
            else:
                raise ValidationError(f'Unknown fragment type: {fragment}')

    def check_fragment_has_unique_name(self, fragment: Fragment) -> None:
        existing_fragment = self._fragments_by_name[fragment.name]
        if fragment is not existing_fragment:
            first_match = text_format.MessageToString(
                existing_fragment, as_one_line=True
            )
            second_match = text_format.MessageToString(
                fragment, as_one_line=True
            )
            raise ValidationError(
                f'The name `{fragment.name}` is shared by the following '
                'configuration elements:\n'
                f'[{first_match}] and '
                f'[{second_match}]'
            )

    @staticmethod
    def check_fragment_has_name(fragment: Fragment) -> None:
        if not fragment.name:
            raise ValidationError(
                'The following configuration element is unnamed:\n'
                + str(fragment)
            )

    def check_config_references_real_build_type(
        self, config: workflows_pb2.BuildConfig
    ) -> None:
        if config.build_type not in self._build_drivers:
            raise ValidationError(
                f'BuildConfig `{config.name}` references unknown build type '
                f'`{config.build_type}`'
            )

    def check_build_references_real_config(
        self, build: workflows_pb2.Build
    ) -> None:
        if (
            build.use_config
            and build.use_config not in self._shared_config_names
        ):
            raise ValidationError(
                f'Build `{build.name}` references missing build config '
                f'`{build.use_config}`'
            )

    @staticmethod
    def check_build_has_config(build: workflows_pb2.Build) -> None:
        which = build.WhichOneof('config')
        if which != 'use_config' and which != 'build_config':
            raise ValidationError(
                f'Build `{build.name}` has no associated configuration'
            )

    @staticmethod
    def check_build_has_targets(build: workflows_pb2.Build) -> None:
        if not build.targets:
            raise ValidationError(f'Build `{build.name}` has no targets.')

    def check_tool_references_real_config(
        self, tool: workflows_pb2.Tool
    ) -> None:
        if tool.use_config and tool.use_config not in self._shared_config_names:
            raise ValidationError(
                f'Tool `{tool.name}` references missing build config '
                f'`{tool.use_config}`'
            )

    @staticmethod
    def check_tool_has_config(tool: workflows_pb2.Tool) -> None:
        which = tool.WhichOneof('config')
        if which != 'use_config' and which != 'build_config':
            raise ValidationError(
                f'Tool `{tool.name}` has no associated configuration'
            )

    @staticmethod
    def check_tool_has_target(tool: workflows_pb2.Tool) -> None:
        if not tool.target:
            raise ValidationError(f'Tool `{tool.name}` has no target.')

    def check_group_builds_exist(self, group: workflows_pb2.TaskGroup) -> None:
        for build_name in group.builds:
            if build_name not in self._fragments_by_name:
                raise ValidationError(
                    f'TaskGroup `{group.name}` references missing build '
                    f'`{build_name}`'
                )

    def check_group_analyzers_exist(
        self, group: workflows_pb2.TaskGroup
    ) -> None:
        for analyzer_name in group.analyzers:
            if analyzer_name not in self._fragments_by_name:
                raise ValidationError(
                    f'TaskGroup `{group.name}` references missing analyzer '
                    f'`{analyzer_name}`'
                )
            fragment = self._fragments_by_name[analyzer_name]
            if not isinstance(fragment, workflows_pb2.Tool):
                raise ValidationError(
                    f'TaskGroup `{group.name}` references analyzer '
                    f'`{analyzer_name}` which is not a tool.'
                )
            if (
                fragment.type != workflows_pb2.Tool.Type.ANALYZER
                and not fragment.analyzer_friendly_args
            ):
                raise ValidationError(
                    f'TaskGroup `{group.name}` references analyzer '
                    f'`{analyzer_name}` which is not of type ANALYZER and '
                    'has no analyzer_friendly_args.'
                )
