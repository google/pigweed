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
"""Bazel driver for workflows.

This tool converts an incoming Bazel BuildDriverRequest as ProtoJSON
into a BuildDriverResponse that contains a sequence of actions that make up
a bazel build.

Example:

$ bazel run //pw_build/py:bazel_build_driver -- \
    --request=$PWD/pw_build/test_data/bazel_build_driver_request.json

"""

import sys

from pw_build.proto import (
    build_driver_pb2,
    workflows_pb2,
    pigweed_build_driver_pb2,
)
from pw_build.workflows.build_driver import BuildDriver


class BazelBuildDriver(BuildDriver):
    """A Workflows build driver for Bazel builds."""

    def generate_action_sequence(
        self, job: build_driver_pb2.JobRequest
    ) -> build_driver_pb2.JobResponse:
        if job.WhichOneof('type') == 'tool':
            return self.generate_action_sequence_for_tool(job.tool)
        if job.WhichOneof('type') == 'build':
            return self.generate_action_sequence_for_build(job.build)

        # This is a nop.
        return build_driver_pb2.JobResponse()

    @staticmethod
    def extra_descriptors():
        return [pigweed_build_driver_pb2.DESCRIPTOR]

    def generate_action_sequence_for_build(
        self, build: workflows_pb2.Build
    ) -> build_driver_pb2.JobResponse:
        actions = build_driver_pb2.JobResponse()
        actions.actions.append(self._canonicalize_args(build.build_config))
        actions.actions.append(
            build_driver_pb2.Action(
                executable='bazelisk',
                args=['build', *build.build_config.args, *build.targets],
                env=build.build_config.env,
                run_from=build_driver_pb2.Action.InvocationLocation.INVOKER_CWD,
            )
        )
        driver_options = self.unpack_driver_options(
            pigweed_build_driver_pb2.BazelDriverOptions,
            build.build_config.driver_options,
        )

        if not driver_options.no_test:
            actions.actions.append(
                build_driver_pb2.Action(
                    executable='bazelisk',
                    args=['test', *build.build_config.args, *build.targets],
                    env=build.build_config.env,
                    run_from=(
                        build_driver_pb2.Action.InvocationLocation.INVOKER_CWD
                    ),
                )
            )
        return actions

    def generate_action_sequence_for_tool(
        self, tool: workflows_pb2.Tool
    ) -> build_driver_pb2.JobResponse:
        actions = build_driver_pb2.JobResponse()
        actions.actions.append(self._canonicalize_args(tool.build_config))
        bazel_run_action = build_driver_pb2.Action(
            executable='bazelisk',
            args=[
                'run',
                *tool.build_config.args,
                tool.target,
                '--',
                '${FORWARDED_LAUNCH_ARGS}',
            ],
            env=tool.build_config.env,
            run_from=build_driver_pb2.Action.InvocationLocation.INVOKER_CWD,
        )
        actions.actions.append(bazel_run_action)
        return actions

    @staticmethod
    def _canonicalize_args(
        build_config: workflows_pb2.BuildConfig,
    ) -> build_driver_pb2.Action:
        """Ensures that Bazel configs don't include target patterns."""
        return build_driver_pb2.Action(
            executable='bazelisk',
            args=['canonicalize-flags', *build_config.args],
            env=build_config.env,
            run_from=build_driver_pb2.Action.InvocationLocation.INVOKER_CWD,
        )


if __name__ == '__main__':
    sys.exit(BazelBuildDriver().main())
