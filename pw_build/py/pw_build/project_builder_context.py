# Copyright 2023 The Pigweed Authors
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
"""Fetch active Project Builder Context."""

import concurrent.futures
from contextvars import ContextVar
from dataclasses import dataclass, field
from enum import Enum
import logging
import subprocess
from typing import Dict, Optional

from pw_build.build_recipe import BuildRecipe


_LOG = logging.getLogger('pw_build.watch')


def _wait_for_terminate_then_kill(
    proc: subprocess.Popen, timeout: int = 3
) -> int:
    """Wait for a process to end, then kill it if the timeout expires."""
    returncode = 1
    try:
        returncode = proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        _LOG.debug('Killing %s', proc)
        proc.kill()
    return returncode


class ProjectBuilderState(Enum):
    IDLE = 'IDLE'
    BUILDING = 'BUILDING'
    ABORT = 'ABORT'


@dataclass
class ProjectBuilderContext:
    """Maintains the state of running builds and active subproccesses."""

    current_state: Optional[ProjectBuilderState] = ProjectBuilderState.IDLE
    desired_state: Optional[ProjectBuilderState] = ProjectBuilderState.BUILDING
    procs: Dict[BuildRecipe, Optional[subprocess.Popen]] = field(
        default_factory=dict
    )

    def register_process(
        self, recipe: BuildRecipe, proc: subprocess.Popen
    ) -> None:
        self.procs[recipe] = proc

    def terminate_and_wait(self) -> None:
        """End a subproces either cleanly or with a kill signal."""
        if self.is_idle() or self.should_abort():
            return

        self._signal_abort()

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=len(self.procs)
        ) as executor:
            futures = []
            for _recipe, proc in self.procs.items():
                if proc is None:
                    continue
                _LOG.debug('Wait for %s', proc)
                futures.append(
                    executor.submit(_wait_for_terminate_then_kill, proc)
                )
            for future in concurrent.futures.as_completed(futures):
                future.result()

        _LOG.debug('Wait for terminate DONE')
        self.set_idle()

    def _signal_abort(self) -> None:
        self.desired_state = ProjectBuilderState.ABORT

    def should_abort(self) -> bool:
        return self.desired_state == ProjectBuilderState.ABORT

    def is_building(self) -> bool:
        return self.current_state == ProjectBuilderState.BUILDING

    def is_idle(self) -> bool:
        return self.current_state == ProjectBuilderState.IDLE

    def set_idle(self) -> None:
        self.current_state = ProjectBuilderState.IDLE
        self.desired_state = ProjectBuilderState.IDLE

    def set_building(self) -> None:
        self.current_state = ProjectBuilderState.BUILDING
        self.desired_state = ProjectBuilderState.BUILDING


PROJECT_BUILDER_CONTEXTVAR = ContextVar(
    'pw_build_project_builder_state', default=ProjectBuilderContext()
)


def get_project_builder_context():
    return PROJECT_BUILDER_CONTEXTVAR.get()
