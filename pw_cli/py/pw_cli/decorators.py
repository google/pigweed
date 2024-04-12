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
"""Common helpful decorators for Python code."""

import inspect
from pathlib import Path
from typing import Callable
import warnings


def deprecated(deprecation_note: str):
    """Deprecation decorator.

    Emits a depreciation warning when the annotated function is used.

    An additional deprecation note is required to redirect users to the
    appropriate alternative.
    """

    def _decorator_impl(func: Callable):
        def _wrapper(*args, **kwargs):
            caller = inspect.getframeinfo(inspect.currentframe().f_back)
            deprecated_func_module = inspect.getmodule(func)
            module_or_file = (
                '.'.join(
                    (
                        deprecated_func_module.__package__,
                        Path(deprecated_func_module.__file__).stem,
                    )
                )
                if deprecated_func_module.__package__ is not None
                else deprecated_func_module.__file__
            )
            warnings.warn(
                ' '.join(
                    (
                        f'{caller.filename}:{caller.lineno}:',
                        f'{module_or_file}.{func.__qualname__}()',
                        'is deprecated.',
                        deprecation_note,
                    )
                ),
                DeprecationWarning,
            )
            return func(*args, **kwargs)

        return _wrapper

    return _decorator_impl
