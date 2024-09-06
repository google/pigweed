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
"""Writes a formatted BUILD.gn _file."""

import os
import subprocess

from datetime import datetime
from pathlib import PurePath, PurePosixPath
from types import TracebackType
from typing import IO, Iterable, Set, Type

from pw_build.gn_target import GnTarget

COPYRIGHT_HEADER = f'''
# Copyright {datetime.now().year} The Pigweed Authors
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

# DO NOT MANUALLY EDIT!'''


class MalformedGnError(Exception):
    """Raised when creating a GN object fails."""


class GnWriter:
    """Represents a partial BUILD.gn file being constructed.

    Except for testing , callers should prefer using `GnFile`. That
    object wraps this one, and ensures that the GN file produced includes the
    relevant copyright header and is formatted correctly.
    """

    def __init__(self, file: IO) -> None:
        self._file: IO = file
        self._scopes: list[str] = []
        self._margin: str = ''

    def write_comment(self, comment: str | None = None) -> None:
        """Adds a GN comment.

        Args:
            comment: The comment string to write.
        """
        if not comment:
            self.write('#')
            return
        while len(comment) > 78:
            index = comment.rfind(' ', 0, 78)
            if index < 0:
                break
            self.write(f'# {comment[:index]}')
            comment = comment[index + 1 :]
        self.write(f'# {comment}')

    def write_import(self, gni: str | PurePosixPath) -> None:
        """Adds a GN import.

        Args:
            gni: The source-relative path to a GN import file.
        """
        self.write(f'import("{str(gni)}")')

    def write_target(self, gn_target: GnTarget) -> None:
        """Write a GN target.

        Args:
            target: The GN target data to write.
        """
        if gn_target.origin:
            self.write_comment(f'Generated from {gn_target.origin}')
        self.write(f'{gn_target.type()}("{gn_target.name()}") {{')
        self._margin += '  '

        # inclusive-language: disable
        # See https://gn.googlesource.com/gn/+/master/docs/style_guide.md
        # inclusive-language: enable
        ordered_names = [
            'public',
            'sources',
            'inputs',
            'cflags',
            'include_dirs',
            'defines',
            'public_configs',
            'configs',
            'public_deps',
            'deps',
        ]
        for attr in ordered_names:
            self.write_list(attr, gn_target.attrs.get(attr, []))
        self._margin = self._margin[2:]
        self.write('}')

    def write_list(self, var_name: str, items: Iterable[str]) -> None:
        """Adds a named GN list of the given items, if non-empty.

        Args:
            var_name: The name of the GN list variable.
            items: The list items to write as strings.
        """
        items = list(items)
        if not items:
            return
        self.write(f'{var_name} = [')
        self._margin += '  '
        for item in sorted(items):
            self.write(f'"{str(item)}",')
        self._margin = self._margin[2:]
        self.write(']')

    def write_blank(self) -> None:
        """Adds a blank line."""
        print('', file=self._file)

    def write_preformatted(self, preformatted: str) -> None:
        """Adds text with minimal formatting.

        The only formatting applied to the given text is to strip any leading
        whitespace. This allows calls to be more readable by allowing
        preformatted text to start on a new line, e.g.

            _write_preformatted('''
          preformatted line 1
          preformatted line 2
          preformatted line 3''')

        Args:
            preformatted: The text to write.
        """
        print(preformatted.lstrip(), file=self._file)

    def write_file(self, imports: Set[str], gn_targets: list[GnTarget]) -> None:
        """Write a complete BUILD.gn file.

        Args:
            imports: A list of GNI files needed by targets.
            gn_targets: A list of GN targets to add to the file.
        """
        self.write_import('//build_overrides/pigweed.gni')
        if imports:
            self.write_blank()
        for gni in sorted(list(imports)):
            self.write_import(gni)
        for target in sorted(gn_targets, key=lambda t: t.name()):
            self.write_blank()
            self.write_target(target)

    def write(self, text: str) -> None:
        """Writes to the file, appropriately indented.

        Args:
            text: The text to indent and write.
        """
        print(f'{self._margin}{text}', file=self._file)


class GnFile:
    """Represents an open BUILD.gn file that is formatted on close.

    Typical usage:

        with GnFile('/path/to/BUILD.gn', 'my-package') as build_gn:
          build_gn.write_...

    where "write_..." refers to any of the "write" methods of `GnWriter`.
    """

    def __init__(self, pathname: PurePath) -> None:
        if pathname.name != 'BUILD.gn' and pathname.suffix != '.gni':
            raise MalformedGnError(f'invalid GN filename: {pathname}')
        os.makedirs(pathname.parent, exist_ok=True)
        self._pathname: PurePath = pathname
        self._file: IO
        self._writer: GnWriter

    def __enter__(self) -> GnWriter:
        """Opens the GN file."""
        self._file = open(self._pathname, 'w+')
        self._writer = GnWriter(self._file)
        self._writer.write_preformatted(COPYRIGHT_HEADER)
        file = PurePath(*PurePath(__file__).parts[-2:])
        self._writer.write_comment(
            f'This file was automatically generated by {file}'
        )
        self._writer.write_blank()
        return self._writer

    def __exit__(
        self,
        exc_type: Type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: TracebackType | None,
    ) -> None:
        """Closes the GN file and formats it."""
        self._file.close()
        subprocess.check_call(['gn', 'format', self._pathname])
