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
"""Functons for checking Python call sites match expected callers."""

from dataclasses import dataclass
from pathlib import Path
import inspect
from typing import Iterable


@dataclass
class AllowedCaller:
    """Container class for storing Python call sites."""

    filename: str
    function: str
    name: str
    self_class: str | None = None

    @staticmethod
    def from_frame_info(frame_info: inspect.FrameInfo) -> 'AllowedCaller':
        """Returns an AllowedCaller based on an inspect.FrameInfo object."""
        self_obj = frame_info.frame.f_locals.get('self', None)
        global_name_str = frame_info.frame.f_globals.get('__name__', None)
        module_class = None
        if self_obj:
            module_class = self_obj.__class__.__name__
        return AllowedCaller(
            filename=frame_info.filename,
            function=frame_info.function,
            name=global_name_str,
            self_class=module_class,
        )

    def matches(self, other: 'AllowedCaller') -> bool:
        """Returns true if this AllowedCaller matches another one."""
        file_matches = Path(other.filename).match(f'**/{self.filename}')
        name_matches = self.name == other.name
        if self.name == '*':
            name_matches = True
        function_matches = self.function == other.function
        final_match = file_matches and name_matches and function_matches

        # If self_class is set, check those values too.
        if self.self_class and other.self_class:
            self_class_matches = self.self_class == other.self_class
            final_match = final_match and self_class_matches

        return final_match

    def __repr__(self) -> str:
        return f'''AllowedCaller(
  filename='{self.filename}',
  name='{self.name}',
  function='{self.function}',
  self_class='{self.self_class}',
)'''


def check_caller_in(allow_list: Iterable[AllowedCaller]) -> bool:
    """Return true if the called function is in the allowed call list.

    Raises a RuntimeError if the call location is not in the allow_list.
    """
    # Get the current Python call stack.
    call_stack = [
        AllowedCaller.from_frame_info(frame_info)
        for frame_info in inspect.stack()
    ]

    called_function = call_stack[1]
    call_location = call_stack[2]

    # Check to see if the caller of this function is allowed.
    caller_is_allowed = any(
        allowed_call.matches(call_location) for allowed_call in allow_list
    )

    if not caller_is_allowed:
        raise RuntimeError(
            '\n\nThis call location is not in the allow list for this '
            'called function.\n\n'
            f'Called function:\n\n{called_function}\n\n'
            f'Call location:\n\n{call_location}\n'
        )

    return caller_is_allowed
