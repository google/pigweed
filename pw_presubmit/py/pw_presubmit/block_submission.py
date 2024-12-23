# Copyright 2024 The Pigweed Authors
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
"""Checks for phrases intended to block submission."""

import re

from . import presubmit, presubmit_context, inclusive_language


@presubmit.check(name='block_submission')
def presubmit_check(ctx: presubmit_context.PresubmitContext) -> None:
    inclusive_language.generic_presubmit_check(
        ctx,
        words_regex=re.compile(
            r"do[\s_-]not[\s_-]submit|don'?t[\s_-]submit", re.IGNORECASE
        ),
        disable_tag='block-submission: disable',
        enable_tag='block-submission: enable',
        ignore_tag='block-submission: ignore',
        issue_type='submission-blocking phrase',
    )
