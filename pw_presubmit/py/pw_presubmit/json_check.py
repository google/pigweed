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
"""JSON validity check."""

import json

from . import presubmit


@presubmit.filter_paths(endswith=('.json',))
@presubmit.check(name='json_check')
def presubmit_check(ctx: presubmit.PresubmitContext):
    """Presubmit check that ensures JSON files are valid."""

    for path in ctx.paths:
        with path.open('r') as ins:
            try:
                json.load(ins)
            except json.decoder.JSONDecodeError as exc:
                intro_line = f'failed to parse {path.relative_to(ctx.root)}'
                with ctx.failure_summary_log.open('w') as outs:
                    print(intro_line, file=outs)
                    print(exc, file=outs)
                ctx.fail(intro_line)
                ctx.fail(str(exc))
