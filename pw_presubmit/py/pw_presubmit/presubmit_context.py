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
import enum
import logging
from pathlib import Path
import shlex
from typing import (
    Any,
    Dict,
    List,
    Iterable,
    NamedTuple,
    Optional,
    TYPE_CHECKING,
)

import pw_cli.color

if TYPE_CHECKING:
    from pw_presubmit.presubmit import PresubmitContext, Check

_COLOR = pw_cli.color.colors()
_LOG: logging.Logger = logging.getLogger(__name__)

PRESUBMIT_CHECK_TRACE: ContextVar[
    Dict[str, List['PresubmitCheckTrace']]
] = ContextVar('pw_presubmit_check_trace', default={})

PRESUBMIT_CONTEXT: ContextVar[Optional['PresubmitContext']] = ContextVar(
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
