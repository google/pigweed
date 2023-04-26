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
"""Utilities for manipulating GN labels and paths."""

from __future__ import annotations

import re

from pathlib import PurePosixPath
from typing import Optional, Union


class MalformedGnError(Exception):
    """Raised when creating a GN object fails."""


class GnPath:
    """Represents a GN source path to a file in the source tree."""

    def __init__(
        self,
        base: Union[str, PurePosixPath, GnPath],
        bazel: Optional[str] = None,
        gn: Optional[str] = None,  # pylint: disable=invalid-name
    ) -> None:
        """Creates a GN source path.

        Args:
            base: A base GN source path. Other parameters are used to define
                this object relative to the base.
            bazel: A Bazel path relative to `base`.
            gn: A GN source path relative to `base`.
        """
        self._path: PurePosixPath
        base_path = _as_path(base)
        if bazel:
            self._from_bazel(base_path, bazel)
        elif gn:
            self._from_gn(base_path, gn)
        else:
            self._path = base_path

    def __str__(self) -> str:
        return str(self._path)

    def _from_bazel(self, base_path: PurePosixPath, label: str) -> None:
        """Populates this object using a Bazel file label.

        A Bazel label looks like:
            //[{package-path}][:{relative-path}]
        e.g.
            "//foo"             => "$base/foo"
            "//:bar/baz.txt"    => "$base/bar/baz.txt"
            "//foo:bar/baz.txt" => "$base/foo/bar/baz.txt"
        """
        match = re.match(r'//([^():]*)(?::([^():]+))?', label)
        if not match:
            raise MalformedGnError(f'invalid path: {label}')
        groups = filter(None, match.groups())
        self._path = base_path.joinpath(*groups)

    def _from_gn(self, base_path: PurePosixPath, path: str) -> None:
        """Populates this object using a GN label.

        Source-relative paths interpreted relative to `base`. Source-absolute
        paths are used directly.
        """
        if path.startswith('//'):
            self._path = PurePosixPath(path)
        else:
            self._path = base_path.joinpath(path)

    def path(self) -> str:
        """Returns the object's path."""
        return str(self._path)

    def file(self) -> str:
        """Like GN's `get_path_info(..., "file")`."""
        return self._path.name

    def name(self) -> str:
        """Like GN's `get_path_info(..., "name")`."""
        return self._path.stem

    def extension(self) -> str:
        """Like GN's `get_path_info(..., "extension")`."""
        suffix = self._path.suffix
        return suffix[1:] if suffix.startswith('.') else suffix

    def dir(self) -> str:
        """Like GN's `get_path_info(..., "dir")`."""
        return str(self._path.parent)


class GnLabel:
    """Represents a GN dependency.

    See https://gn.googlesource.com/gn/+/main/docs/reference.md#labels.
    """

    def __init__(
        self,
        base: Union[str, PurePosixPath, GnLabel],
        public: bool = False,
        bazel: Optional[str] = None,
        gn: Optional[str] = None,  # pylint: disable=invalid-name
    ) -> None:
        """Creates a GN label.

        Args:
            base: A base GN label. Other parameters are used to define this
                object relative to the base.
            public: When this label is used to refer to a GN `dep`, this flag
                indicates if it should be a `public_dep`.
            bazel: A Bazel label relative to `base`.
            gn: A GN label relative to `base`.
        """
        self._name: str
        self._path: PurePosixPath
        self._toolchain: Optional[str] = None
        self._public: bool = public
        self._repo: Optional[str] = None
        base_path = _as_path(base)
        if bazel:
            self._from_bazel(base_path, bazel)
        elif gn:
            self._from_gn(base_path, gn)
        elif ':' in str(base_path):
            parts = str(base_path).split(':')
            self._path = PurePosixPath(':'.join(parts[:-1]))
            self._name = parts[-1]
        else:
            self._path = base_path
            self._name = self._path.name

    def __str__(self):
        return self.with_toolchain() if self._toolchain else self.no_toolchain()

    def __eq__(self, other):
        return str(self) == str(other)

    def __hash__(self):
        return hash(str(self))

    def _from_bazel(self, base: PurePosixPath, label: str):
        """Populates this object using a Bazel label."""
        match = re.match(r'(?:@([^():/]*))?//(.+)', label)
        if not match:
            raise MalformedGnError(f'invalid label: {label}')
        self._repo = match[1]
        if self._repo:
            self._from_gn(PurePosixPath('$repo'), match[2])
        else:
            self._from_gn(base, match[2])

    def _from_gn(self, base: PurePosixPath, label: str):
        """Populates this object using a GN label."""
        if label.startswith('//') or label.startswith('$'):
            path = label
        else:
            path = str(base.joinpath(label))
        if ':' in path:
            parts = path.split(':')
            self._path = PurePosixPath(':'.join(parts[:-1]))
            self._name = parts[-1]
        else:
            self._path = PurePosixPath(path)
            self._name = self._path.name
        parts = []
        for part in self._path.parts:
            if part == '..' and parts and parts[-1] != '..':
                parts.pop()
            else:
                parts.append(part)
        self._path = PurePosixPath(*parts)

    def name(self) -> str:
        """Like GN's `get_label_info(..., "name"`)."""
        return self._name

    def dir(self) -> str:
        """Like GN's `get_label_info(..., "dir"`)."""
        return str(self._path)

    def no_toolchain(self) -> str:
        """Like GN's `get_label_info(..., "label_no_toolchain"`)."""
        if self._path == PurePosixPath():
            return f':{self._name}'
        name = f':{self._name}' if self._name != self._path.name else ''
        return f'{self._path}{name}'

    def with_toolchain(self) -> str:
        """Like GN's `get_label_info(..., "label_with_toolchain"`)."""
        toolchain = self._toolchain if self._toolchain else 'default_toolchain'
        if self._path == PurePosixPath():
            return f':{self._name}({toolchain})'
        name = f':{self._name}' if self._name != self._path.name else ''
        return f'{self._path}{name}({toolchain})'

    def public(self) -> bool:
        """Returns whether this is a public dep."""
        return self._public

    def repo(self) -> str:
        """Returns the label's repo, if any."""
        return self._repo or ''

    def resolve_repo(self, repo: str) -> None:
        """Replaces the repo placeholder with the given value."""
        if self._path and self._path.parts[0] == '$repo':
            self._path = PurePosixPath(
                '$dir_pw_third_party', repo, *self._path.parts[1:]
            )

    def relative_to(self, start: Union[str, PurePosixPath, GnLabel]) -> str:
        """Returns a label string relative to the given starting label."""
        start_path = _as_path(start)
        if not start:
            return self.no_toolchain()
        if self._path == start_path:
            return f':{self._name}'
        path = _relative_to(self._path, start_path)
        name = f':{self._name}' if self._name != self._path.name else ''
        return f'{path}{name}'

    def joinlabel(self, relative: str) -> GnLabel:
        """Creates a new label by extending the current label."""
        return GnLabel(self._path.joinpath(relative))


class GnVisibility:
    """Represents a GN visibility scope."""

    def __init__(
        self,
        base: Union[str, PurePosixPath, GnLabel],
        label: Union[str, PurePosixPath, GnLabel],
        bazel: Optional[str] = None,
        gn: Optional[str] = None,  # pylint: disable=invalid-name
    ) -> None:
        """Creates a GN visibility scope.

        Args:
            base: A base GN label. Other parameters are used to define this
                object relative to the base.
            label: The label of the directory in which this scope is being
                defined.
            bazel: An absolute Bazel visibility label.
            gn: A GN visibility label.
        """
        self._scope: GnLabel
        label_path = _as_path(label)
        if bazel:
            self._from_bazel(_as_path(base), label_path, bazel)
        elif gn:
            self._from_gn(label_path, gn)
        else:
            self._scope = GnLabel(label_path)

    def __str__(self):
        return str(self._scope)

    def _from_bazel(
        self, base: PurePosixPath, label: PurePosixPath, scope: str
    ):
        """Populates this object using a Bazel visibility label."""
        if scope == '//visibility:public':
            self._scope = GnLabel('//*')
        elif scope == '//visibility:private':
            self._scope = GnLabel(label, gn=':*')
        elif not (match := re.match(r'//([^():]*):([^():]+)', scope)):
            raise MalformedGnError(f'invalid visibility scope: {scope}')
        elif match[2] == '__subpackages__':
            self._scope = GnLabel(base, gn=f'{match[1]}/*')
        elif match[2] == '__pkg__':
            self._scope = GnLabel(base, gn=f'{match[1]}:*')
        else:
            raise MalformedGnError(f'unsupported visibility scope: {scope}')

    def _from_gn(self, label: PurePosixPath, scope: str):
        """Populates this object using a GN visibility scope."""
        self._scope = GnLabel(label, gn=scope)

    def relative_to(self, start: Union[str, PurePosixPath, GnLabel]) -> str:
        """Returns a label string relative to the given starting label."""
        return self._scope.relative_to(start)


def _as_path(item: Union[str, GnPath, GnLabel, PurePosixPath]) -> PurePosixPath:
    """Converts an argument to be a PurePosixPath.

    Args:
        label: A string, path, or label to be converted to a PurePosixPath.
    """
    if isinstance(item, str):
        return PurePosixPath(item)
    if isinstance(item, GnPath):
        return PurePosixPath(item.path())
    if isinstance(item, GnLabel):
        return PurePosixPath(item.dir())
    return item


def _relative_to(
    path: Union[str, PurePosixPath], start: Union[str, PurePosixPath]
) -> PurePosixPath:
    """Like `PosixPath._relative_to`, but can ascend directories as well."""
    if not start:
        return PurePosixPath(path)
    _path = PurePosixPath(path)
    _start = PurePosixPath(start)
    if _path.parts[0] != _start.parts[0]:
        return _path
    ascend = PurePosixPath()
    while True:
        try:
            return ascend.joinpath(_path.relative_to(_start))
        except ValueError:
            if _start.parent == PurePosixPath():
                raise
            _start = _start.parent
            ascend = ascend.joinpath('..')
