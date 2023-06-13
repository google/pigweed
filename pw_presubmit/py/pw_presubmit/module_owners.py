# Copyright 2022 The Pigweed Authors
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
"""Ensure all modules have OWNERS files."""

import logging
from pathlib import Path
from typing import Callable, Optional, Tuple

from pw_presubmit.presubmit_context import (
    PresubmitContext,
    PresubmitFailure,
)
from pw_presubmit import presubmit

_LOG: logging.Logger = logging.getLogger(__name__)


def upstream_pigweed_applicability(
    ctx: PresubmitContext,
    path: Path,
) -> Optional[Path]:
    """Return a parent of path required to have an OWNERS file, or None."""
    parts: Tuple[str, ...] = path.relative_to(ctx.root).parts

    if len(parts) >= 2 and parts[0].startswith('pw_'):
        return ctx.root / parts[0]
    if len(parts) >= 3 and parts[0] in ('targets', 'third_party'):
        return ctx.root / parts[0] / parts[1]

    return None


ApplicabilityFunc = Callable[[PresubmitContext, Path], Optional[Path]]


def presubmit_check(
    applicability: ApplicabilityFunc = upstream_pigweed_applicability,
) -> presubmit.Check:
    """Create a presubmit check for the presence of OWNERS files."""

    @presubmit.check(name='module_owners')
    def check(ctx: PresubmitContext) -> None:
        """Presubmit check that ensures all modules have OWNERS files."""

        modules_to_check = set()

        for path in ctx.paths:
            result = applicability(ctx, path)
            if result:
                modules_to_check.add(result)

        errors = 0
        for module in sorted(modules_to_check):
            _LOG.debug('Checking module %s', module)
            owners_path = module / 'OWNERS'
            if not owners_path.is_file():
                _LOG.error('%s is missing an OWNERS file', module)
                errors += 1
                continue

            with owners_path.open() as ins:
                contents = [x.strip() for x in ins.read().strip().splitlines()]
                wo_comments = [x for x in contents if not x.startswith('#')]
                owners = [x for x in wo_comments if 'per-file' not in x]
                if len(owners) < 1:
                    _LOG.error('%s is too short: add owners', owners_path)

        if errors:
            raise PresubmitFailure

    return check
