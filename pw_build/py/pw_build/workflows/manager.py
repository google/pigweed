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
"""A library for running tools, builds, and more from a workflows.json file."""

from __future__ import annotations
from collections import defaultdict
from collections.abc import Sequence
import os
from pathlib import Path
import re
import string

from pw_build.build_recipe import BuildCommand, BuildRecipe
from pw_build.proto import build_driver_pb2, workflows_pb2
from pw_build.workflows.build_driver import BuildDriver
from pw_build.workflows.private.validator import (
    Fragment,
    Validator,
    collect_all_fragments,
)

# Regex for template/variable expansion.
_TEMPLATE_PATTERN = re.compile(r'(?:\${?)([a-zA-Z_][a-zA-Z0-9_]*)')

_BAZEL_PROJECT_ROOT_ENV_VAR = 'BUILD_WORKSPACE_DIRECTORY'
_BOOTSTRAP_PROJECT_ROOT_ENV_VAR = 'PW_PROJECT_ROOT'


def expand_action(
    action: build_driver_pb2.Action,
    env: dict[str, str | list[str]],
) -> list[str]:
    """Expands environment variables in Action arguments.

    This differs from os.path.expandvars() and string.Template in one key way:
    instances of ${FOO} that expand to a list of strings are inserted at the
    location of the variable. For example:

    Variables: {'BAR': ['c', 'd']}
    Arguments: ['a', 'b', '$BAR', 'e']

    Expanded result: ['a', 'b', 'c', 'd', 'e']

    Otherwise, the variable expansion behaves identically to
    os.path.expandvars().

    Note: This will not fail if an environment variable isn't known.
    """
    expanded_args: list[str] = []
    for arg in action.args:
        # When Python 3.11 is the minimum Python version, this can be
        # rewritten in terms of Template.get_identifiers().
        matches = _TEMPLATE_PATTERN.findall(arg)
        is_list_expansion = any(
            isinstance(env.get(match, None), list) for match in matches
        )
        if is_list_expansion:
            if len(matches) > 1:
                raise ValueError('Cannot expand pattern into multiple values')
            expanded_args.extend(env[matches[0]])
        else:
            expanded_args.append(string.Template(arg).safe_substitute(env))
    return expanded_args


class WorkflowsManager:
    """A class for creating build recipes from a workflows.json file.

    This class is responsible for validating Workflows configuration files,
    and converting them into executable build actions. Execution of builds
    is left to the caller to support diversity in build execution schemes.
    """

    def __init__(
        self,
        workflow_suite: workflows_pb2.WorkflowSuite,
        build_drivers: dict[str, BuildDriver],
        working_dir: Path,
        base_out_dir: Path,
        project_root: Path | None = None,
    ):
        self._workflow_suite = workflow_suite
        self._build_drivers = build_drivers
        self._fragments_by_name: dict[str, Fragment] = {}
        self._all_fragments = collect_all_fragments(self._workflow_suite)
        self._fragments_by_name = {
            fragment.name: fragment for fragment in self._all_fragments
        }
        self._shared_configs: dict[str, workflows_pb2.BuildConfig] = {
            config.name: config for config in self._workflow_suite.configs
        }
        if project_root is not None:
            self._project_root = project_root
        elif _BAZEL_PROJECT_ROOT_ENV_VAR in os.environ:
            self._project_root = Path(os.environ[_BAZEL_PROJECT_ROOT_ENV_VAR])
        elif _BOOTSTRAP_PROJECT_ROOT_ENV_VAR in os.environ:
            self._project_root = Path(
                os.environ[_BOOTSTRAP_PROJECT_ROOT_ENV_VAR]
            )
        else:
            raise ValueError('Cannot locate project root')
        self._base_out_dir = base_out_dir
        self._working_dir = working_dir
        Validator(workflow_suite, build_drivers).validate()

    def _get_build_config(
        self,
        fragment: Fragment,
    ) -> workflows_pb2.BuildConfig:
        if not isinstance(fragment, (workflows_pb2.Tool, workflows_pb2.Build)):
            raise TypeError(
                'Cannot get a build config from something that is not a build '
                f'or tool: {fragment}'
            )
        if fragment.WhichOneof('config') == 'use_config':
            return self._shared_configs[fragment.use_config]
        return fragment.build_config

    def _action_working_dir(
        self,
        run_from: build_driver_pb2.Action.InvocationLocation.ValueType,
        build_dir: Path,
    ) -> Path:
        if run_from == build_driver_pb2.Action.InvocationLocation.PROJECT_ROOT:
            return self._project_root
        if run_from == build_driver_pb2.Action.InvocationLocation.INVOKER_CWD:
            return self._working_dir
        return build_dir

    def _postprocess_job_response(
        self,
        job_response: build_driver_pb2.JobResponse,
        build_dir: Path,
        forwarded_args: Sequence[str],
    ) -> None:
        """Updates the incoming JobResponse to expand all variables."""
        for action in job_response.actions:
            working_dir = self._action_working_dir(
                action.run_from,
                build_dir,
            )
            # TODO: https://pwbug.dev/428715231 - Define these elsewhere
            # in a way that makes documentation generation of this list
            # scalable/maintainable.
            substitutions: dict[str, str | list[str]] = {
                'FORWARDED_LAUNCH_ARGS': list(forwarded_args),
                'BUILD_ROOT': str(
                    os.path.relpath(build_dir.resolve(), working_dir.resolve())
                ),
                'PROJECT_ROOT': str(
                    os.path.relpath(
                        self._project_root.resolve(),
                        working_dir.resolve(),
                    )
                ),
                'INVOKER_DIR': str(
                    os.path.relpath(
                        self._working_dir.resolve(),
                        working_dir.resolve(),
                    )
                ),
            }
            expanded_args = expand_action(action, substitutions)
            action.ClearField('args')
            action.args.extend(expanded_args)

    def _create_build_recipes(
        self,
        fragments: list[Fragment],
        forwarded_args: Sequence[str] | None = None,
    ) -> list[BuildRecipe]:
        """Creates build recipes for a series of configuration fragments."""
        if forwarded_args is None:
            forwarded_args = []

        build_driver_requests: dict[
            str, build_driver_pb2.BuildDriverRequest
        ] = defaultdict(build_driver_pb2.BuildDriverRequest)
        for fragment in fragments:
            if isinstance(fragment, workflows_pb2.Tool):
                config = self._get_build_config(fragment)
                job_request = build_driver_pb2.JobRequest(
                    tool=fragment,
                )
                build_driver_requests[config.build_type].jobs.append(
                    job_request
                )
            elif isinstance(fragment, workflows_pb2.Build):
                config = self._get_build_config(fragment)
                job_request = build_driver_pb2.JobRequest(
                    build=fragment,
                )
                build_driver_requests[config.build_type].jobs.append(
                    job_request
                )
            else:
                raise ValueError(
                    'Tried to create a build from an unsupported fragment '
                    f'type: {fragment}'
                )

        build_recipes = []
        for build_type, request in build_driver_requests.items():
            driver = self._build_drivers[build_type]
            response = driver.generate_jobs(
                self._prepare_driver_request(request)
            )
            for job_request, job_response in zip(request.jobs, response.jobs):
                build_dir = self._base_out_dir / config.name
                fragment = (
                    job_request.tool
                    if job_request.WhichOneof('type') == 'tool'
                    else job_request.build
                )
                config = self._get_build_config(
                    self._fragments_by_name[fragment.name]
                )
                steps = []
                self._postprocess_job_response(
                    job_response=job_response,
                    build_dir=build_dir,
                    forwarded_args=forwarded_args,
                )
                for action in job_response.actions:
                    working_dir = self._action_working_dir(
                        action.run_from,
                        build_dir,
                    )
                    targets = (
                        [fragment.target]
                        if hasattr(fragment, 'target')
                        else list(fragment.targets)
                    )
                    steps.append(
                        BuildCommand(
                            command=[
                                action.executable,
                                *action.args,
                            ],
                            working_dir=working_dir,
                            targets=targets,
                        )
                    )
                build_recipes.append(
                    BuildRecipe(
                        build_dir=build_dir,
                        title=config.name,
                        steps=steps,
                    )
                )
        return build_recipes

    def _prepare_driver_request(
        self,
        request: build_driver_pb2.BuildDriverRequest,
    ) -> build_driver_pb2.BuildDriverRequest:
        """Prepares a build_driver_pb2.BuildDriverRequest.

        This helper takes an incoming build_driver_pb2.BuildDriverRequest and
        prepares it so that a build driver has all the information it needs.

        * Copies the incoming request.
        * Expands referenced shared build configs.
        * Removes info that build drivers shouldn't see/use from a request.

        Returns:
            A properly prepared copy of the incoming
                build_driver_pb2.BuildDriverRequest.
        """
        sanitized = build_driver_pb2.BuildDriverRequest()
        for job in request.jobs:
            sanitized_job = build_driver_pb2.JobRequest()
            sanitized_job.CopyFrom(job)
            fragment = (
                sanitized_job.build
                if sanitized_job.WhichOneof('type') == 'build'
                else sanitized_job.tool
            )
            if fragment.WhichOneof('config') == 'use_config':
                config = self._get_build_config(fragment)
                fragment.ClearField('use_config')
                fragment.build_config.CopyFrom(config)

            fragment.ClearField('name')
            fragment.ClearField('description')
            fragment.build_config.ClearField('name')
            fragment.build_config.ClearField('description')
            sanitized.jobs.append(sanitized_job)
        return sanitized

    def program_tool(
        self,
        tool_name: str,
        forwarded_arguments: Sequence[str],
        as_analyzer: bool = False,
    ) -> list[BuildRecipe]:
        """Generates build recipes for a workflows_pb2.Tool by name.

        Args:
            tool_name: The name of the Tool to launch.
            forwarded_arguments: Arguments to forward when launching the
                underlying tool.
            as_analyzer: If true, launches the requested tool in analyzer
                mode (if supported). This tells the tool to not modify
                any in-tree sources.

        Returns:
            A list of BuildRecipes that fulfill this request.
        """
        tool = self._fragments_by_name.get(tool_name, None)
        if not isinstance(tool, workflows_pb2.Tool):
            raise TypeError(f'{tool_name} is not a tool.')
        if as_analyzer:
            if (
                tool.type != workflows_pb2.Tool.Type.ANALYZER
                and not tool.analyzer_friendly_args
            ):
                raise TypeError(
                    f'Tool `{tool_name}` cannot be run as an analyzer. '
                    'Either add `analyzer_friendly_args` or ensure the tool '
                    'is safe to run as an analyzer'
                )
            forwarded_arguments = [
                *tool.analyzer_friendly_args,
                *forwarded_arguments,
            ]
        return self._create_build_recipes([tool], forwarded_arguments)

    def program_build(self, build_name: str) -> list[BuildRecipe]:
        """Generates build recipes for a workflows_pb2.Build by name.

        Args:
            build_name: The name of the Build to launch.

        Returns:
            A list of BuildRecipes that fulfill this request.
        """
        build = self._fragments_by_name.get(build_name, None)
        if not isinstance(build, workflows_pb2.Build):
            raise TypeError(f'{build_name} is not a build.')
        return self._create_build_recipes([build])

    def program_group(self, group_name: str) -> list[BuildRecipe]:
        """Generates build recipes for a workflows_pb2.TaskGroup by name.

        Args:
            group_name: The name of the Group to launch.

        Returns:
            A list of BuildRecipes that fulfill this request.
        """
        group = self._fragments_by_name.get(group_name, None)
        if not isinstance(group, workflows_pb2.TaskGroup):
            raise TypeError(f'{group_name} is not a group.')

        builds = [
            self._fragments_by_name[build_name] for build_name in group.builds
        ]
        recipes = self._create_build_recipes(builds)

        for tool_name in group.analyzers:
            tool = self._fragments_by_name[tool_name]
            if not isinstance(tool, workflows_pb2.Tool):
                raise TypeError(f'{tool_name} is not a tool.')
            recipes.extend(
                self._create_build_recipes(
                    [tool],
                    list(tool.analyzer_friendly_args),
                )
            )

        return recipes
