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
from pathlib import Path, PurePath, PurePosixPath
from types import TracebackType
from typing import IO, Iterable, Iterator, Optional, Type

from pw_build.gn_config import GnConfig, GN_CONFIG_FLAGS
from pw_build.gn_target import GnTarget
from pw_build.gn_utils import GnLabel, GnPath, MalformedGnError

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


class GnWriter:
    """Represents a partial BUILD.gn file being constructed.

    Except for testing , callers should prefer using `GnFile`. That
    object wraps this one, and ensures that the GN file produced includes the
    relevant copyright header and is formatted correctly.

    Attributes:
        repos: A mapping of repository names to build args. These are used to
            replace repository names when writing labels.
        aliases: A mapping of label names to build args. These can be used to
            rewrite labels with alternate names, e.g. "gtest" to "googletest".
    """

    def __init__(self, file: IO) -> None:
        self._file: IO = file
        self._scopes: list[str] = []
        self._margin: str = ''
        self._needs_blank: bool = False
        self.repos: dict[str, str] = {}
        self.aliases: dict[str, str] = {}

    def write_comment(self, comment: Optional[str] = None) -> None:
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

    def write_import(self, gni: str | PurePosixPath | GnPath) -> None:
        """Adds a GN import.

        Args:
            gni: The source-relative path to a GN import file.
        """
        self._needs_blank = False
        self.write(f'import("{str(gni)}")')
        self._needs_blank = True

    def write_imports(self, imports: Iterable[str]) -> None:
        """Adds a list of GN imports.

        Args:
            imports: A list of GN import files.
        """
        for gni in imports:
            self.write_import(gni)

    def write_config(self, config: GnConfig) -> None:
        """Adds a GN config.

        Args:
            config: The GN config data to write.
        """
        if not config:
            return
        if not config.label:
            raise MalformedGnError('missing label for `config`')
        self.write_target_start('config', config.label.name())
        for flag in GN_CONFIG_FLAGS:
            self.write_list(flag, config.get(flag))
        self.write_end()

    def write_target(self, target: GnTarget) -> None:
        """Write a GN target.

        Args:
            target: The GN target data to write.
        """
        self.write_comment(
            f'Generated from //{target.package()}:{target.name()}'
        )
        self.write_target_start(target.type(), target.name())

        # GN use no `visibility` to indicate publicly visibile.
        scopes = filter(lambda s: str(s) != '//*', target.visibility)
        visibility = [target.make_relative(scope) for scope in scopes]
        self.write_list('visibility', visibility)

        if not target.check_includes:
            self.write('check_includes = false')
        self.write_list('public', [str(path) for path in target.public])
        self.write_list('sources', [str(path) for path in target.sources])
        self.write_list('inputs', [str(path) for path in target.inputs])

        for flag in GN_CONFIG_FLAGS:
            self.write_list(flag, target.config.get(flag))
        self._write_relative('public_configs', target, target.public_configs)
        self._write_relative('configs', target, target.configs)
        self._write_relative('remove_configs', target, target.remove_configs)

        self._write_relative('public_deps', target, target.public_deps)
        self._write_relative('deps', target, target.deps)
        self.write_end()

    def _write_relative(
        self, var_name: str, target: GnTarget, labels: Iterable[GnLabel]
    ) -> None:
        """Write a list of labels relative to a target.

        Args:
            var_name: The name of the GN list variable.
            target: The GN target to rebase the labels to.
            labels: The labels to write to the list.
        """
        self.write_list(var_name, self._resolve(target, labels))

    def _resolve(
        self, target: GnTarget, labels: Iterable[GnLabel]
    ) -> Iterator[str]:
        """Returns rewritten labels.

        If this label has a repo, it must be a key in this object's `repos` and
        will be replaced by the corresponding value. If this label  is a key in
        this object's `aliases`, it will be replaced by the corresponding value.

        Args:
            labels: The labels to resolve.
        """
        for label in labels:
            repo = label.repo()
            if repo:
                label.resolve_repo(self.repos[repo])
            label = GnLabel(self.aliases.get(str(label), str(label)))
            yield target.make_relative(label)

    def write_target_start(
        self, target_type: str, target_name: Optional[str] = None
    ) -> None:
        """Begins a GN target of the given type.

        Args:
            target_type: The type of the GN target.
            target_name: The name of the GN target.
        """
        if target_name:
            self.write(f'{target_type}("{target_name}") {{')
            self._indent(target_name)
        else:
            self.write(f'{target_type}() {{')
            self._indent(target_type)

    def write_list(
        self, var_name: str, items: Iterable[str], reorder: bool = True
    ) -> None:
        """Adds a named GN list of the given items, if non-empty.

        Args:
            var_name: The name of the GN list variable.
            items: The list items to write as strings.
            reorder: If true, the list is sorted lexicographically.
        """
        items = list(items)
        if not items:
            return
        self.write(f'{var_name} = [')
        self._indent(var_name)
        if reorder:
            items = sorted(items)
        for item in items:
            self.write(f'"{str(item)}",')
        self._outdent()
        self.write(']')

    def write_scope(self, var_name: str) -> None:
        """Begins a named GN scope.

        Args:
            var_name: The name of the GN scope variable.
        """
        self.write(f'{var_name} = {{')
        self._indent(var_name)

    def write_if(self, cond: str) -> None:
        """Begins a GN 'if' condition.

        Args:
            cond: The conditional expression.
        """
        self.write(f'if ({cond}) {{')
        self._indent(cond)

    def write_else_if(self, cond: str) -> None:
        """Adds another GN 'if' condition to a previous 'if' condition.

        Args:
            cond: The conditional expression.
        """
        self._outdent()
        self.write(f'}} else if ({cond}) {{')
        self._indent(cond)

    def write_else(self) -> None:
        """Adds a GN 'else' clause to a previous 'if' condition."""
        last = self._outdent()
        self.write('} else {')
        self._indent(f'!({last})')

    def write_end(self) -> None:
        """Ends a target, scope, or 'if' condition'."""
        self._outdent()
        self.write('}')
        self._needs_blank = True

    def write_blank(self) -> None:
        """Adds a blank line."""
        print('', file=self._file)
        self._needs_blank = False

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

    def write(self, text: str) -> None:
        """Writes to the file, appropriately indented.

        Args:
            text: The text to indent and write.
        """
        if self._needs_blank:
            self.write_blank()
        print(f'{self._margin}{text}', file=self._file)

    def _indent(self, scope: str) -> None:
        """Increases the current margin.

        Saves the scope of indent to aid in debugging. For example, trying to
        use incorrect code such as

        ```
          self.write_if('foo')
          self.write_comment('bar')
          self.write_else_if('baz')
        ```

        will throw an exception due to the missing `write_end`. The exception
        will note that 'baz' was opened but not closed.

        Args:
            scope: The name of the scope (for debugging).
        """
        self._scopes.append(scope)
        self._margin += '  '

    def _outdent(self) -> str:
        """Decreases the current margin."""
        if not self._scopes:
            raise MalformedGnError('scope closed unexpectedly')
        last = self._scopes.pop()
        self._margin = self._margin[2:]
        self._needs_blank = False
        return last

    def seal(self) -> None:
        """Instructs the object that no more writes will occur."""
        if self._scopes:
            raise MalformedGnError(f'unclosed scope(s): {self._scopes}')


def gn_format(gn_file: Path) -> None:
    """Calls `gn format` on a BUILD.gn or GN import file."""
    subprocess.check_call(['gn', 'format', gn_file])


class GnFile:
    """Represents an open BUILD.gn file that is formatted on close.

    Typical usage:

        with GnFile('/path/to/BUILD.gn', 'my-package') as build_gn:
          build_gn.write_...

    where "write_..." refers to any of the "write" methods of `GnWriter`.
    """

    def __init__(
        self, pathname: PurePath, package: Optional[str] = None
    ) -> None:
        if pathname.name != 'BUILD.gn' and pathname.suffix != '.gni':
            raise MalformedGnError(f'invalid GN filename: {pathname}')
        os.makedirs(pathname.parent, exist_ok=True)
        self._pathname: PurePath = pathname
        self._package: Optional[str] = package
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
        if self._package:
            self._writer.write_comment(
                f'It contains GN build targets for {self._package}.'
            )
        self._writer.write_blank()
        return self._writer

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> None:
        """Closes the GN file and formats it."""
        self._file.close()
        gn_format(Path(self._pathname))
