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
"""A CLI tool for running tools, builds, and more from a workflows.json file."""

import argparse
from collections.abc import Callable, Sequence
import json
import logging
import os
from pathlib import Path
import sys
from typing import NoReturn

from google.protobuf import json_format, message, text_format
from pw_build import project_builder
from pw_build.proto import workflows_pb2
from pw_build.workflows.bazel_driver import BazelBuildDriver
from pw_build.workflows.manager import WorkflowsManager
from pw_cli import multitool
from pw_config_loader import find_config

_LOG = logging.getLogger(__name__)
_PROJECT_BUILDER_LOGGER = logging.getLogger(f'{_LOG.name}.project_builder')

_WORKFLOWS_FILE = 'workflows.json'


class _BuiltinPlugin(multitool.MultitoolPlugin):
    def __init__(
        self,
        name: str,
        description: str,
        callback: Callable[[Sequence[str]], int],
    ):
        self._name = name
        self._description = description
        self._callback = callback

    def name(self) -> str:
        return self._name

    def help(self) -> str:
        return self._description

    def run(self, plugin_args: Sequence[str]) -> int:
        return self._callback(plugin_args)


class _WorkflowToolPlugin(multitool.MultitoolPlugin):
    def __init__(
        self,
        fragment: workflows_pb2.Tool,
        manager: WorkflowsManager,
    ):
        self._fragment = fragment
        self._manager = manager

    def name(self) -> str:
        return self._fragment.name

    def help(self) -> str:
        return self._fragment.description

    def run(self, plugin_args: Sequence[str]) -> int:
        if isinstance(self._fragment, workflows_pb2.Tool):
            recipes = self._manager.program_tool(self.name(), plugin_args)
        else:
            recipes = self._manager.program_group(self.name())

        # Don't forward project builder output to stdout when launching a
        # tool, it pollutes tool output.
        _PROJECT_BUILDER_LOGGER.propagate = False
        return project_builder.run_builds(
            project_builder.ProjectBuilder(
                build_recipes=recipes,
                execute_command=project_builder.execute_command_pure,
                root_logger=_PROJECT_BUILDER_LOGGER,
            ),
        )


class _WorkflowGroupPlugin(multitool.MultitoolPlugin):
    def __init__(
        self,
        fragment: workflows_pb2.TaskGroup,
        manager: WorkflowsManager,
    ):
        self._fragment = fragment
        self._manager = manager

    def name(self) -> str:
        return self._fragment.name

    def help(self) -> str:
        return self._fragment.description

    def run(self, plugin_args: Sequence[str]) -> int:
        parser = argparse.ArgumentParser(
            prog=f'pw {self.name()}',
            description=self.help(),
        )
        step_choices = [
            *self._fragment.builds,
            *self._fragment.analyzers,
        ]
        parser.add_argument(
            '--step',
            type=str,
            metavar='STEP_NAME',
            choices=step_choices,
            help=(
                'Isolates the execution of a single step in this workflow. '
                'Choices: ' + ', '.join(step_choices)
            ),
        )
        parser.add_argument(
            '--list-steps',
            nargs='?',
            metavar='FILE_PATH',
            const=sys.stdout,
            type=argparse.FileType('w'),
            help=(
                'Lists the steps in this group. If this is set to a file path, '
                'the list of steps are written to a file.'
            ),
        )
        args = parser.parse_args(plugin_args)

        if args.list_steps:
            args.list_steps.write('\n'.join(step_choices))
            args.list_steps.write('\n')
            return 0

        if args.step:
            recipes = self._manager.program_by_name(args.step)
            _PROJECT_BUILDER_LOGGER.propagate = True
            builder = project_builder.ProjectBuilder(
                build_recipes=recipes,
                root_logger=_PROJECT_BUILDER_LOGGER,
            )
            return builder.run_builds()

        recipes = self._manager.program_group(self.name())
        _PROJECT_BUILDER_LOGGER.propagate = True
        builder = project_builder.ProjectBuilder(
            build_recipes=recipes,
            root_logger=_PROJECT_BUILDER_LOGGER,
        )
        return builder.run_builds()


class WorkflowsCli(multitool.MultitoolCli):
    """A CLI entry point for launching project-specific workflows."""

    def __init__(self, config: workflows_pb2.WorkflowSuite | None = None):
        super().__init__()
        self.config: workflows_pb2.WorkflowSuite | None = config
        self._workflows: WorkflowsManager | None = None

    @staticmethod
    def _load_proto_json(config: Path) -> workflows_pb2.WorkflowSuite:
        json_msg = json.loads(config.read_text())
        msg = workflows_pb2.WorkflowSuite()
        json_format.ParseDict(json_msg, msg)
        return msg

    @staticmethod
    def _load_config_from(
        search_from: Path = Path.cwd(),
    ) -> workflows_pb2.WorkflowSuite:
        config = next(
            find_config.configs_in_parents(_WORKFLOWS_FILE, search_from),
            None,
        )
        if not config:
            _LOG.critical(
                'No `%s` file found in current directory or its parents',
                _WORKFLOWS_FILE,
            )
            return workflows_pb2.WorkflowSuite()
        return WorkflowsCli._load_proto_json(config)

    def add_arguments(self, parser: argparse.ArgumentParser) -> None:
        super().add_arguments(parser)
        parser.add_argument(
            '--output-dir',
            '-o',
            default=Path('out'),
            type=Path,
            help=(
                'Output root for builds triggered by launched workflows. '
                'Builds and tools will be nested in configuration-specific '
                'subdirectories.'
            ),
        )

    def _dump_textproto(self, plugin_args: Sequence[str]) -> int:
        parser = argparse.ArgumentParser(
            description=(
                'Describes subsets or expanded views of the current '
                'workflows configuration.'
            ),
        )
        parser.add_argument(
            'name',
            nargs='+',
            default=None,
            help=(
                'The name of a build, tool, group, or build configuration to '
                'inspect. By default, this will emit the requested items as '
                'TextProto.'
            ),
        )
        parser.add_argument(
            '--dump-build-requests',
            nargs='?',
            metavar='FILE_PATH',
            const=sys.stdout,
            type=argparse.FileType('w'),
            help=(
                'Emits all build driver requests produced by the requested '
                'items as a TextProto BuildDriverRequest message.'
            ),
        )
        args = parser.parse_args(plugin_args)
        if self.config is None:
            print('Config is empty')
            return 0
        if not args.name:
            print(self.dump_config())
        elif args.dump_build_requests:
            print(
                self.dump_build_request(args.name),
                file=args.dump_build_requests,
            )
        else:
            for name in args.name:
                print(self.dump_fragment(name))
        return 0

    def dump_config(self) -> str:
        """Dumps the entire config in a human-readable format."""
        if self.config is None:
            return ''

        return text_format.MessageToBytes(self.config).decode()

    def dump_build_request(self, names: Sequence[str]) -> str:
        """Dumps the unified build driver request for this config fragment."""
        assert self._workflows is not None
        return text_format.MessageToBytes(
            self._workflows.get_unified_driver_request(names, sanitize=False)
        ).decode()

    def dump_fragment(self, fragment_name: str) -> str:
        """Dumps a fragment of the config in a human-readable format."""
        if not fragment_name:
            raise ValueError('Invalid empty fragment name')
        dump: message.Message | None = None

        if self.config is not None:
            for conf in self.config.configs:
                if conf.name == fragment_name:
                    dump = conf
                    break
            for tool in self.config.tools:
                if tool.name == fragment_name:
                    dump = tool
                    break
                if tool.build_config.name == fragment_name:
                    dump = tool.build_config
                    break
            for build in self.config.builds:
                if build.name == fragment_name:
                    dump = build
                    break
            for group in self.config.groups:
                if group.name == fragment_name:
                    dump = group
                    break
        if dump is None:
            raise ValueError(
                f'Could not find any config fragment named `{fragment_name}`'
            )
        return text_format.MessageToBytes(dump).decode()

    def _launch_analyzer(self, args: Sequence[str]) -> int:
        if self._workflows is None:
            raise AssertionError(
                'Internal error: failed to initialize workflows manager'
            )

        # Don't forward project builder output to stdout when launching a
        # tool, it pollutes tool output.
        _PROJECT_BUILDER_LOGGER.propagate = False
        return project_builder.run_builds(
            project_builder.ProjectBuilder(
                build_recipes=self._workflows.program_tool(
                    args[0], args[1:], as_analyzer=True
                ),
                execute_command=project_builder.execute_command_pure,
            ),
        )

    def _launch_build(self, args: Sequence[str]) -> int:
        if self._workflows is None:
            raise AssertionError(
                'Internal error: failed to initialize workflows manager'
            )
        _PROJECT_BUILDER_LOGGER.propagate = False
        return project_builder.run_builds(
            project_builder.ProjectBuilder(
                build_recipes=self._workflows.program_build(args[0]),
                root_logger=_PROJECT_BUILDER_LOGGER,
            )
        )

    def _builtin_plugins(self) -> list[multitool.MultitoolPlugin]:
        return [
            _BuiltinPlugin(
                name='build',
                description='Launch one or more builds',
                callback=self._launch_build,
            ),
            _BuiltinPlugin(
                name='describe',
                description='Describe a build, tool, or group',
                callback=self._dump_textproto,
            ),
            _BuiltinPlugin(
                name='check',
                description='Launch a tool in a no-modify mode',
                callback=self._launch_analyzer,
            ),
        ]

    def plugins(
        self, args: argparse.Namespace
    ) -> Sequence[multitool.MultitoolPlugin]:
        if not self.config:
            self.config = self._load_config_from()
        self._workflows = WorkflowsManager(
            self.config,
            {
                "bazel": BazelBuildDriver(),
            },
            working_dir=Path.cwd(),
            base_out_dir=args.output_dir,
        )

        all_plugins = []
        all_plugins.extend(self._builtin_plugins())
        all_plugins.extend(
            [_WorkflowToolPlugin(t, self._workflows) for t in self.config.tools]
        )
        all_plugins.extend(
            [
                _WorkflowGroupPlugin(g, self._workflows)
                for g in self.config.groups
            ]
        )

        # Helper to work around mypy bug limitation for the lambda pattern of
        # argument freezing (https://github.com/python/mypy/issues/12557).
        def create_build_callback(
            build: workflows_pb2.Build,
        ) -> Callable[[Sequence[str]], int]:
            return lambda _: self._launch_build([build.name])

        for build in self.config.builds:
            if not build.rerun_shortcut:
                continue
            all_plugins.append(
                _BuiltinPlugin(
                    name=build.rerun_shortcut,
                    description=build.description,
                    callback=create_build_callback(build),
                )
            )

        #
        def create_tool_callback(
            tool: workflows_pb2.Tool,
        ) -> Callable[[Sequence[str]], int]:
            return lambda args: self._launch_analyzer([tool.name, *args])

        for tool in self.config.tools:
            if not tool.rerun_shortcut:
                continue
            all_plugins.append(
                _BuiltinPlugin(
                    name=tool.rerun_shortcut,
                    description=f'(Analyzer) {tool.description}',
                    callback=create_tool_callback(tool),
                )
            )

        return all_plugins

    def main(self) -> NoReturn:
        # Small bit of UX magic: The `pw` wrapper entry point emits a loading
        # message while this tool is being bootstrapped. As soon as this tool
        # launches, clear that line so the output isn't polluted.
        if sys.stdout.isatty():
            print('\r\033[2K', end='')
        if 'BUILD_WORKING_DIRECTORY' in os.environ:
            os.chdir(os.environ['BUILD_WORKING_DIRECTORY'])
        super().main()


if __name__ == '__main__':
    WorkflowsCli().main()
