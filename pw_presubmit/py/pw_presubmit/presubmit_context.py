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
"""pw_presubmit ContextVar."""

from contextvars import ContextVar
import dataclasses
import enum
import inspect
import logging
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
from typing import (
    Any,
    Dict,
    List,
    Iterable,
    NamedTuple,
    Optional,
    Sequence,
    Tuple,
    TYPE_CHECKING,
)
import urllib

import pw_cli.color
import pw_env_setup.config_file

if TYPE_CHECKING:
    from pw_presubmit.presubmit import Check

_COLOR = pw_cli.color.colors()
_LOG: logging.Logger = logging.getLogger(__name__)

PRESUBMIT_CHECK_TRACE: ContextVar[
    Dict[str, List['PresubmitCheckTrace']]
] = ContextVar('pw_presubmit_check_trace', default={})


@dataclasses.dataclass(frozen=True)
class FormatOptions:
    python_formatter: Optional[str] = 'yapf'
    black_path: Optional[str] = 'black'
    exclude: Sequence[re.Pattern] = dataclasses.field(default_factory=list)

    @staticmethod
    def load(env: Optional[Dict[str, str]] = None) -> 'FormatOptions':
        config = pw_env_setup.config_file.load(env=env)
        fmt = config.get('pw', {}).get('pw_presubmit', {}).get('format', {})
        return FormatOptions(
            python_formatter=fmt.get('python_formatter', 'yapf'),
            black_path=fmt.get('black_path', 'black'),
            exclude=tuple(re.compile(x) for x in fmt.get('exclude', ())),
        )


def get_buildbucket_info(bbid) -> Dict[str, Any]:
    if not bbid or not shutil.which('bb'):
        return {}

    output = subprocess.check_output(
        ['bb', 'get', '-json', '-p', f'{bbid}'], text=True
    )
    return json.loads(output)


@dataclasses.dataclass
class LuciPipeline:
    round: int
    builds_from_previous_iteration: Sequence[str]

    @staticmethod
    def create(
        bbid: int,
        fake_pipeline_props: Optional[Dict[str, Any]] = None,
    ) -> Optional['LuciPipeline']:
        pipeline_props: Dict[str, Any]
        if fake_pipeline_props is not None:
            pipeline_props = fake_pipeline_props
        else:
            pipeline_props = (
                get_buildbucket_info(bbid)
                .get('input', {})
                .get('properties', {})
                .get('$pigweed/pipeline', {})
            )
        if not pipeline_props.get('inside_a_pipeline', False):
            return None

        return LuciPipeline(
            round=int(pipeline_props['round']),
            builds_from_previous_iteration=list(
                pipeline_props['builds_from_previous_iteration']
            ),
        )


@dataclasses.dataclass
class LuciTrigger:
    """Details the pending change or submitted commit triggering the build."""

    number: int
    remote: str
    project: str
    branch: str
    ref: str
    gerrit_name: str
    submitted: bool

    @property
    def gerrit_url(self):
        if not self.number:
            return self.gitiles_url
        return 'https://{}-review.googlesource.com/c/{}'.format(
            self.gerrit_name, self.number
        )

    @property
    def gitiles_url(self):
        return '{}/+/{}'.format(self.remote, self.ref)

    @staticmethod
    def create_from_environment(
        env: Optional[Dict[str, str]] = None,
    ) -> Sequence['LuciTrigger']:
        if not env:
            env = os.environ.copy()
        raw_path = env.get('TRIGGERING_CHANGES_JSON')
        if not raw_path:
            return ()
        path = Path(raw_path)
        if not path.is_file():
            return ()

        result = []
        with open(path, 'r') as ins:
            for trigger in json.load(ins):
                keys = {
                    'number',
                    'remote',
                    'project',
                    'branch',
                    'ref',
                    'gerrit_name',
                    'submitted',
                }
                if keys <= trigger.keys():
                    result.append(LuciTrigger(**{x: trigger[x] for x in keys}))

        return tuple(result)

    @staticmethod
    def create_for_testing():
        change = {
            'number': 123456,
            'remote': 'https://pigweed.googlesource.com/pigweed/pigweed',
            'project': 'pigweed/pigweed',
            'branch': 'main',
            'ref': 'refs/changes/56/123456/1',
            'gerrit_name': 'pigweed',
            'submitted': True,
        }
        with tempfile.TemporaryDirectory() as tempdir:
            changes_json = Path(tempdir) / 'changes.json'
            with changes_json.open('w') as outs:
                json.dump([change], outs)
            env = {'TRIGGERING_CHANGES_JSON': changes_json}
            return LuciTrigger.create_from_environment(env)


@dataclasses.dataclass
class LuciContext:
    """LUCI-specific information about the environment."""

    buildbucket_id: int
    build_number: int
    project: str
    bucket: str
    builder: str
    swarming_server: str
    swarming_task_id: str
    cas_instance: str
    pipeline: Optional[LuciPipeline]
    triggers: Sequence[LuciTrigger] = dataclasses.field(default_factory=tuple)

    @staticmethod
    def create_from_environment(
        env: Optional[Dict[str, str]] = None,
        fake_pipeline_props: Optional[Dict[str, Any]] = None,
    ) -> Optional['LuciContext']:
        """Create a LuciContext from the environment."""

        if not env:
            env = os.environ.copy()

        luci_vars = [
            'BUILDBUCKET_ID',
            'BUILDBUCKET_NAME',
            'BUILD_NUMBER',
            'SWARMING_TASK_ID',
            'SWARMING_SERVER',
        ]
        if any(x for x in luci_vars if x not in env):
            return None

        project, bucket, builder = env['BUILDBUCKET_NAME'].split(':')

        bbid: int = 0
        pipeline: Optional[LuciPipeline] = None
        try:
            bbid = int(env['BUILDBUCKET_ID'])
            pipeline = LuciPipeline.create(bbid, fake_pipeline_props)

        except ValueError:
            pass

        # Logic to identify cas instance from swarming server is derived from
        # https://chromium.googlesource.com/infra/luci/recipes-py/+/main/recipe_modules/cas/api.py
        swarm_server = env['SWARMING_SERVER']
        cas_project = urllib.parse.urlparse(swarm_server).netloc.split('.')[0]
        cas_instance = f'projects/{cas_project}/instances/default_instance'

        result = LuciContext(
            buildbucket_id=bbid,
            build_number=int(env['BUILD_NUMBER']),
            project=project,
            bucket=bucket,
            builder=builder,
            swarming_server=env['SWARMING_SERVER'],
            swarming_task_id=env['SWARMING_TASK_ID'],
            cas_instance=cas_instance,
            pipeline=pipeline,
            triggers=LuciTrigger.create_from_environment(env),
        )
        _LOG.debug('%r', result)
        return result

    @staticmethod
    def create_for_testing():
        env = {
            'BUILDBUCKET_ID': '881234567890',
            'BUILDBUCKET_NAME': 'pigweed:bucket.try:builder-name',
            'BUILD_NUMBER': '123',
            'SWARMING_SERVER': 'https://chromium-swarm.appspot.com',
            'SWARMING_TASK_ID': 'cd2dac62d2',
        }
        return LuciContext.create_from_environment(env, {})


@dataclasses.dataclass
class FormatContext:
    """Context passed into formatting helpers.

    This class is a subset of PresubmitContext containing only what's needed by
    formatters.

    For full documentation on the members see the PresubmitContext section of
    pw_presubmit/docs.rst.

    Args:
        root: Source checkout root directory
        output_dir: Output directory for this specific language
        paths: Modified files for the presubmit step to check (often used in
            formatting steps but ignored in compile steps)
        package_root: Root directory for pw package installations
        format_options: Formatting options, derived from pigweed.json
    """

    root: Optional[Path]
    output_dir: Path
    paths: Tuple[Path, ...]
    package_root: Path
    format_options: FormatOptions
    dry_run: bool = False

    def append_check_command(self, *command_args, **command_kwargs) -> None:
        """Empty append_check_command."""


class PresubmitFailure(Exception):
    """Optional exception to use for presubmit failures."""

    def __init__(
        self,
        description: str = '',
        path: Optional[Path] = None,
        line: Optional[int] = None,
    ):
        line_part: str = ''
        if line is not None:
            line_part = f'{line}:'
        super().__init__(
            f'{path}:{line_part} {description}' if path else description
        )


@dataclasses.dataclass
class PresubmitContext:  # pylint: disable=too-many-instance-attributes
    """Context passed into presubmit checks.

    For full documentation on the members see pw_presubmit/docs.rst.

    Args:
        root: Source checkout root directory
        repos: Repositories (top-level and submodules) processed by
            pw presubmit
        output_dir: Output directory for this specific presubmit step
        failure_summary_log: Path where steps should write a brief summary of
            any failures encountered for use by other tooling.
        paths: Modified files for the presubmit step to check (often used in
            formatting steps but ignored in compile steps)
        all_paths: All files in the tree.
        package_root: Root directory for pw package installations
        override_gn_args: Additional GN args processed by build.gn_gen()
        luci: Information about the LUCI build or None if not running in LUCI
        format_options: Formatting options, derived from pigweed.json
        num_jobs: Number of jobs to run in parallel
        continue_after_build_error: For steps that compile, don't exit on the
            first compilation error
        rng_seed: Seed for a random number generator, for the few steps that
            need one
        full: Whether this is a full or incremental presubmit run
    """

    root: Path
    repos: Tuple[Path, ...]
    output_dir: Path
    failure_summary_log: Path
    paths: Tuple[Path, ...]
    all_paths: Tuple[Path, ...]
    package_root: Path
    luci: Optional[LuciContext]
    override_gn_args: Dict[str, str]
    format_options: FormatOptions
    num_jobs: Optional[int] = None
    continue_after_build_error: bool = False
    rng_seed: int = 1
    full: bool = False
    _failed: bool = False
    dry_run: bool = False

    @property
    def failed(self) -> bool:
        return self._failed

    @property
    def incremental(self) -> bool:
        return not self.full

    def fail(
        self,
        description: str,
        path: Optional[Path] = None,
        line: Optional[int] = None,
    ):
        """Add a failure to this presubmit step.

        If this is called at least once the step fails, but not immediately—the
        check is free to continue and possibly call this method again.
        """
        _LOG.warning('%s', PresubmitFailure(description, path, line))
        self._failed = True

    @staticmethod
    def create_for_testing():
        parsed_env = pw_cli.env.pigweed_environment()
        root = parsed_env.PW_PROJECT_ROOT
        presubmit_root = root / 'out' / 'presubmit'
        return PresubmitContext(
            root=root,
            repos=(root,),
            output_dir=presubmit_root / 'test',
            failure_summary_log=presubmit_root / 'failure-summary.log',
            paths=(root / 'foo.cc', root / 'foo.py'),
            all_paths=(root / 'BUILD.gn', root / 'foo.cc', root / 'foo.py'),
            package_root=root / 'environment' / 'packages',
            luci=None,
            override_gn_args={},
            format_options=FormatOptions(),
        )

    def append_check_command(
        self,
        *command_args,
        call_annotation: Optional[Dict[Any, Any]] = None,
        **command_kwargs,
    ) -> None:
        """Save a subprocess command annotation to this presubmit context.

        This is used to capture commands that will be run for display in ``pw
        presubmit --dry-run.``

        Args:

            command_args: All args that would normally be passed to
                subprocess.run

            call_annotation: Optional key value pairs of data to save for this
                command. Examples:

                ::

                   call_annotation={'pw_package_install': 'teensy'}
                   call_annotation={'build_system': 'bazel'}
                   call_annotation={'build_system': 'ninja'}

            command_kwargs: keyword args that would normally be passed to
                subprocess.run.
        """
        call_annotation = call_annotation if call_annotation else {}
        calling_func: Optional[str] = None
        calling_check = None

        # Loop through the current call stack looking for `self`, and stopping
        # when self is a Check() instance and if the __call__ or _try_call
        # functions are in the stack.

        # This used to be an isinstance(obj, Check) call, but it was changed to
        # this so Check wouldn't need to be imported here. Doing so would create
        # a dependency loop.
        def is_check_object(obj):
            return getattr(obj, '_is_presubmit_check_object', False)

        for frame_info in inspect.getouterframes(inspect.currentframe()):
            self_obj = frame_info.frame.f_locals.get('self', None)
            if (
                self_obj
                and is_check_object(self_obj)
                and frame_info.function in ['_try_call', '__call__']
            ):
                calling_func = frame_info.function
                calling_check = self_obj

        save_check_trace(
            self.output_dir,
            PresubmitCheckTrace(
                self,
                calling_check,
                calling_func,
                command_args,
                command_kwargs,
                call_annotation,
            ),
        )

    def __post_init__(self) -> None:
        PRESUBMIT_CONTEXT.set(self)

    def __hash__(self):
        return hash(
            tuple(
                tuple(attribute.items())
                if isinstance(attribute, dict)
                else attribute
                for attribute in dataclasses.astuple(self)
            )
        )


PRESUBMIT_CONTEXT: ContextVar[Optional[PresubmitContext]] = ContextVar(
    'pw_presubmit_context', default=None
)


def get_presubmit_context():
    return PRESUBMIT_CONTEXT.get()


class PresubmitCheckTraceType(enum.Enum):
    BAZEL = 'BAZEL'
    CMAKE = 'CMAKE'
    GN_NINJA = 'GN_NINJA'
    PW_PACKAGE = 'PW_PACKAGE'


class PresubmitCheckTrace(NamedTuple):
    ctx: 'PresubmitContext'
    check: Optional['Check']
    func: Optional[str]
    args: Iterable[Any]
    kwargs: Dict[Any, Any]
    call_annotation: Dict[Any, Any]

    def __repr__(self) -> str:
        return f'''CheckTrace(
  ctx={self.ctx.output_dir}
  id(ctx)={id(self.ctx)}
  check={self.check}
  args={self.args}
  kwargs={self.kwargs.keys()}
  call_annotation={self.call_annotation}
)'''


def save_check_trace(output_dir: Path, trace: PresubmitCheckTrace) -> None:
    trace_key = str(output_dir.resolve())
    trace_list = PRESUBMIT_CHECK_TRACE.get().get(trace_key, [])
    trace_list.append(trace)
    PRESUBMIT_CHECK_TRACE.get()[trace_key] = trace_list


def get_check_traces(ctx: 'PresubmitContext') -> List[PresubmitCheckTrace]:
    trace_key = str(ctx.output_dir.resolve())
    return PRESUBMIT_CHECK_TRACE.get().get(trace_key, [])


def log_check_traces(ctx: 'PresubmitContext') -> None:
    traces = PRESUBMIT_CHECK_TRACE.get()

    for _output_dir, check_traces in traces.items():
        for check_trace in check_traces:
            if check_trace.ctx != ctx:
                continue

            quoted_command_args = ' '.join(
                shlex.quote(str(arg)) for arg in check_trace.args
            )
            _LOG.info(
                '%s %s',
                _COLOR.blue('Run ==>'),
                quoted_command_args,
            )


def apply_exclusions(
    ctx: PresubmitContext,
    paths: Optional[Sequence[Path]] = None,
) -> Tuple[Path, ...]:
    root = Path(pw_cli.env.pigweed_environment().PW_PROJECT_ROOT)
    relpaths = [x.relative_to(root) for x in paths or ctx.paths]

    for filt in ctx.format_options.exclude:
        relpaths = [x for x in relpaths if not filt.search(str(x))]
    return tuple(root / x for x in relpaths)
