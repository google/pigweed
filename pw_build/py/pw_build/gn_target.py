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
"""Utilities for manipulating GN targets."""

import re

from typing import Iterable


class GnTarget:
    """Represents a Pigweed Build target.

    Attributes:
        attrs:  Map of GN variable names to lists of values. Recognized GN list
                variables include 'public', 'sources', 'inputs', 'deps', etc.
        origin: Reference to the data used to generate this target, e.g. a
                Bazel label.
    """

    def __init__(self, target_type: str, target_name: str) -> None:
        """Creates a GN target.

        Args:
            target_type: Type of the GN target, like 'pw_source_set'.
            target_name: Name of the GN target.
        """
        self.attrs: dict[str, list[str]] = {}
        self.origin: str | None = None
        self._type = target_type
        self._name = target_name

    def type(self) -> str:
        """Returns this object's GN target_type."""
        return self._type

    def name(self) -> str:
        """Returns this object's GN target_name."""
        return self._name

    def build_args(self) -> Iterable[str]:
        """Returns a list of GN build arguments referenced in paths.

        Third party source code is referenced by Pigweed build files using build
        arguments, e.g. 'public = [ "$dir_pw_third_party_foo/bar/baz.h" ]'. This
        method returns any build arguments used to locate source, header or
        input files so that the BUILD.gn file can import the build argument
        definition.
        """
        build_arg_pat = re.compile(r'(\$dir_pw_third_party_[^/:]*)')
        for field in ['public', 'sources', 'inputs', 'include_dirs']:
            for source_path in self.attrs.get(field, []):
                match = re.search(build_arg_pat, source_path)
                if match:
                    yield match.group(1)
