# Copyright 2021 The Pigweed Authors
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
"""C++-related checks."""

from pw_presubmit import PresubmitContext, PresubmitFailure, filter_paths


@filter_paths(endswith=('.h', '.hpp', '.hxx', '.hh', '.H'),
              exclude=(r'\.pb\.h$', ))
def pragma_once(ctx: PresubmitContext) -> None:
    """Presubmit check that ensures all header files contain '#pragma once'."""

    for path in ctx.paths:
        with open(path) as file:
            for line in file:
                if line.startswith('#pragma once'):
                    break
            else:
                raise PresubmitFailure('#pragma once is missing!', path=path)
