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

import asyncio
import concurrent.futures
from contextvars import ContextVar
from datetime import datetime
from dataclasses import dataclass, field
from enum import Enum
import logging
import os
import subprocess
from typing import Callable, Dict, List, Optional, NoReturn, TYPE_CHECKING

from prompt_toolkit.formatted_text import (
    AnyFormattedText,
    StyleAndTextTuples,
)
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.layout.dimension import AnyDimension, D
from prompt_toolkit.layout import (
    AnyContainer,
    DynamicContainer,
    FormattedTextControl,
    Window,
)
from prompt_toolkit.shortcuts import ProgressBar, ProgressBarCounter
from prompt_toolkit.shortcuts.progress_bar.formatters import (
    Formatter,
    TimeElapsed,
)
from prompt_toolkit.shortcuts.progress_bar import formatters

from pw_build.build_recipe import BuildRecipe

if TYPE_CHECKING:
    from pw_build.project_builder import ProjectBuilder

_LOG = logging.getLogger('pw_build.watch')


def _wait_for_terminate_then_kill(
    proc: subprocess.Popen, timeout: int = 5
) -> int:
    """Wait for a process to end, then kill it if the timeout expires."""
    returncode = 1
    try:
        returncode = proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc_command = proc.args
        if isinstance(proc.args, list):
            proc_command = ' '.join(proc.args)
        _LOG.debug('Killing %s', proc_command)
        proc.kill()
    return returncode


class ProjectBuilderState(Enum):
    IDLE = 'IDLE'
    BUILDING = 'BUILDING'
    ABORT = 'ABORT'


# pylint: disable=unused-argument
# Prompt Toolkit progress bar formatter classes follow:
class BuildStatus(Formatter):
    """Return OK/FAIL status and last output line for each build."""

    def __init__(self, ctx) -> None:
        self.ctx = ctx

    def format(
        self,
        progress_bar: ProgressBar,
        progress: ProgressBarCounter,
        width: int,
    ) -> AnyFormattedText:
        for cfg in self.ctx.recipes:
            if cfg.display_name != progress.label:
                continue

            build_status: StyleAndTextTuples = []
            build_status.append(
                cfg.status.status_slug(restarting=self.ctx.restart_flag)
            )
            build_status.append(('', ' '))
            build_status.extend(cfg.status.current_step_formatted())

            return build_status

        return [('', '')]

    def get_width(  # pylint: disable=no-self-use
        self, progress_bar: ProgressBar
    ) -> AnyDimension:
        return D()


class TimeElapsedIfStarted(TimeElapsed):
    """Display the elapsed time if the build has started."""

    def __init__(self, ctx) -> None:
        self.ctx = ctx

    def format(
        self,
        progress_bar: ProgressBar,
        progress: ProgressBarCounter,
        width: int,
    ) -> AnyFormattedText:
        formatted_text: StyleAndTextTuples = [('', '')]
        for cfg in self.ctx.recipes:
            if cfg.display_name != progress.label:
                continue
            if cfg.status.started:
                return super().format(progress_bar, progress, width)
        return formatted_text


# pylint: enable=unused-argument


@dataclass
class ProjectBuilderContext:  # pylint: disable=too-many-instance-attributes,too-many-public-methods
    """Maintains the state of running builds and active subproccesses."""

    current_state: ProjectBuilderState = ProjectBuilderState.IDLE
    desired_state: ProjectBuilderState = ProjectBuilderState.BUILDING
    procs: Dict[BuildRecipe, subprocess.Popen] = field(default_factory=dict)
    recipes: List[BuildRecipe] = field(default_factory=list)

    def __post_init__(self) -> None:
        self.project_builder: Optional['ProjectBuilder'] = None

        self.progress_bar_formatters = [
            formatters.Text(' '),
            formatters.Label(),
            formatters.Text(' '),
            BuildStatus(self),
            formatters.Text(' '),
            TimeElapsedIfStarted(self),
            formatters.Text(' '),
        ]

        self._enter_callback: Optional[Callable] = None

        key_bindings = KeyBindings()

        @key_bindings.add('enter')
        def _enter_pressed(_event):
            """Run enter press function."""
            if self._enter_callback:
                self._enter_callback()

        self.key_bindings = key_bindings

        self.progress_bar: Optional[ProgressBar] = None

        self._progress_bar_started: bool = False

        self.bottom_toolbar: AnyFormattedText = None
        self.horizontal_separator = '━'
        self.title_bar_container: AnyContainer = Window(
            char=self.horizontal_separator, height=1
        )

        self.using_fullscreen: bool = False
        self.restart_flag: bool = False
        self.ctrl_c_pressed: bool = False

    def using_progress_bars(self) -> bool:
        return bool(self.progress_bar) or self.using_fullscreen

    def interrupted(self) -> bool:
        return self.ctrl_c_pressed or self.restart_flag

    def set_bottom_toolbar(self, text: AnyFormattedText) -> None:
        self.bottom_toolbar = text

    def set_enter_callback(self, callback: Callable) -> None:
        self._enter_callback = callback

    def ctrl_c_interrupt(self) -> None:
        """Abort function for when using ProgressBars."""
        self.ctrl_c_pressed = True
        self.exit(1)

    def startup_progress(self) -> None:
        self.progress_bar = ProgressBar(
            formatters=self.progress_bar_formatters,
            key_bindings=self.key_bindings,
            title=self.get_title_bar_text,
            bottom_toolbar=self.bottom_toolbar,
            cancel_callback=self.ctrl_c_interrupt,
        )
        self.progress_bar.__enter__()

        self.create_title_bar_container()
        self.progress_bar.app.layout.container.children[  # type: ignore
            0
        ] = DynamicContainer(lambda: self.title_bar_container)
        self._progress_bar_started = True

    def exit_progress(self) -> None:
        if not self.progress_bar:
            return
        self.progress_bar.__exit__()

    def clear_progress_scrollback(self) -> None:
        if not self.progress_bar:
            return
        self.progress_bar._app_loop.call_soon_threadsafe(  # pylint: disable=protected-access
            self.progress_bar.app.renderer.clear
        )

    def redraw_progress(self) -> None:
        if not self.progress_bar:
            return
        if hasattr(self.progress_bar, 'app'):
            self.progress_bar.invalidate()

    def get_title_style(self) -> str:
        if self.restart_flag:
            return 'fg:ansiyellow'

        # Assume passing
        style = 'fg:ansigreen'

        if self.current_state == ProjectBuilderState.BUILDING:
            style = 'fg:ansiyellow'

        for cfg in self.recipes:
            if cfg.status.failed():
                style = 'fg:ansired'

        return style

    def exit_code(self) -> int:
        """Returns a 0 for success, 1 for fail."""
        for cfg in self.recipes:
            if cfg.status.failed():
                return 1
        return 0

    def get_title_bar_text(
        self, include_separators: bool = True
    ) -> StyleAndTextTuples:
        title = ''

        fail_count = 0
        done_count = 0
        for cfg in self.recipes:
            if cfg.status.failed():
                fail_count += 1
            if cfg.status.done:
                done_count += 1

        if self.restart_flag:
            title = 'INTERRUPT'
        elif fail_count > 0:
            title = f'FAILED ({fail_count})'
        elif self.current_state == ProjectBuilderState.IDLE and done_count > 0:
            title = 'PASS'
        else:
            title = self.current_state.name

        prefix = ''
        if include_separators:
            prefix += f'{self.horizontal_separator}{self.horizontal_separator} '

        return [(self.get_title_style(), f'{prefix}{title} ')]

    def create_title_bar_container(self) -> None:
        title_text = FormattedTextControl(self.get_title_bar_text)
        self.title_bar_container = Window(
            title_text,
            char=self.horizontal_separator,
            height=1,
            # Expand width to max available space
            dont_extend_width=False,
            style=self.get_title_style,
        )

    def add_progress_bars(self) -> None:
        if not self._progress_bar_started:
            self.startup_progress()
        assert self.progress_bar
        self.clear_progress_bars()
        for cfg in self.recipes:
            self.progress_bar(label=cfg.display_name, total=len(cfg.steps))

    def clear_progress_bars(self) -> None:
        if not self.progress_bar:
            return
        self.progress_bar.counters = []

    def mark_progress_step_complete(self, recipe: BuildRecipe) -> None:
        if not self.progress_bar:
            return
        for pbc in self.progress_bar.counters:
            if pbc.label == recipe.display_name:
                pbc.item_completed()
                break

    def mark_progress_done(self, recipe: BuildRecipe) -> None:
        if not self.progress_bar:
            return

        for pbc in self.progress_bar.counters:
            if pbc.label == recipe.display_name:
                pbc.done = True
                break

    def mark_progress_started(self, recipe: BuildRecipe) -> None:
        if not self.progress_bar:
            return

        for pbc in self.progress_bar.counters:
            if pbc.label == recipe.display_name:
                pbc.start_time = datetime.now()
                break

    def register_process(
        self, recipe: BuildRecipe, proc: subprocess.Popen
    ) -> None:
        self.procs[recipe] = proc

    def terminate_and_wait(
        self,
        exit_message: Optional[str] = None,
    ) -> None:
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

                proc_command = proc.args
                if isinstance(proc.args, list):
                    proc_command = ' '.join(proc.args)
                _LOG.debug('Stopping: %s', proc_command)

                futures.append(
                    executor.submit(_wait_for_terminate_then_kill, proc)
                )
            for future in concurrent.futures.as_completed(futures):
                future.result()

        if exit_message:
            _LOG.info(exit_message)
        self.set_idle()

    def _signal_abort(self) -> None:
        self.desired_state = ProjectBuilderState.ABORT

    def build_stopping(self) -> bool:
        """Return True if the build is restarting or quitting."""
        return self.should_abort() or self.interrupted()

    def should_abort(self) -> bool:
        """Return True if the build is restarting."""
        return self.desired_state == ProjectBuilderState.ABORT

    def is_building(self) -> bool:
        return self.current_state == ProjectBuilderState.BUILDING

    def is_idle(self) -> bool:
        return self.current_state == ProjectBuilderState.IDLE

    def set_project_builder(self, project_builder) -> None:
        self.project_builder = project_builder
        self.recipes = project_builder.build_recipes

    def set_idle(self) -> None:
        self.current_state = ProjectBuilderState.IDLE
        self.desired_state = ProjectBuilderState.IDLE

    def set_building(self) -> None:
        self.restart_flag = False
        self.current_state = ProjectBuilderState.BUILDING
        self.desired_state = ProjectBuilderState.BUILDING

    def restore_stdout_logging(self) -> None:  # pylint: disable=no-self-use
        if not self.using_progress_bars():
            return

        # Restore logging to STDOUT
        stdout_handler = logging.StreamHandler()
        if self.project_builder:
            stdout_handler.setLevel(self.project_builder.default_log_level)
        else:
            stdout_handler.setLevel(logging.INFO)
        root_logger = logging.getLogger()
        if self.project_builder and self.project_builder.stdout_proxy:
            self.project_builder.stdout_proxy.flush()
            self.project_builder.stdout_proxy.close()
        root_logger.addHandler(stdout_handler)

    def restore_logging_and_shutdown(
        self,
        log_after_shutdown: Optional[Callable[[], None]] = None,
    ) -> None:
        self.restore_stdout_logging()
        _LOG.warning('Abort signal recieved, stopping processes...')
        if log_after_shutdown:
            log_after_shutdown()
        self.terminate_and_wait()
        # Flush all log handlers
        # logging.shutdown()

    def exit(
        self,
        exit_code: int = 1,
        log_after_shutdown: Optional[Callable[[], None]] = None,
    ) -> None:
        """Exit function called when the user presses ctrl-c."""

        if not self.progress_bar:
            self.restore_logging_and_shutdown(log_after_shutdown)
            os._exit(exit_code)  # pylint: disable=protected-access

        # Shut everything down after the progress_bar exits.
        def _really_exit(future: asyncio.Future) -> NoReturn:
            self.restore_logging_and_shutdown(log_after_shutdown)
            os._exit(future.result())  # pylint: disable=protected-access

        if self.progress_bar.app.future:
            self.progress_bar.app.future.add_done_callback(_really_exit)
            self.progress_bar.app.exit(result=exit_code)  # type: ignore


PROJECT_BUILDER_CONTEXTVAR = ContextVar(
    'pw_build_project_builder_state', default=ProjectBuilderContext()
)


def get_project_builder_context():
    return PROJECT_BUILDER_CONTEXTVAR.get()
