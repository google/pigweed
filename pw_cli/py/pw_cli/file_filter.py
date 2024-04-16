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
"""Class for describing file filter patterns."""

from pathlib import Path
import re
from typing import Pattern, Iterable, Sequence


class FileFilter:
    """Allows checking if a path matches a series of filters.

    Positive filters (e.g. the file name matches a regex) and negative filters
    (path does not match a regular expression) may be applied.
    """

    def __init__(
        self,
        *,
        exclude: Iterable[Pattern | str] = (),
        endswith: Iterable[str] = (),
        name: Iterable[Pattern | str] = (),
        suffix: Iterable[str] = (),
    ) -> None:
        """Creates a FileFilter with the provided filters.

        Args:
            endswith: True if the end of the path is equal to any of the passed
                      strings
            exclude: If any of the passed regular expresion match return False.
                     This overrides and other matches.
            name: Regexs to match with file names(pathlib.Path.name). True if
                  the resulting regex matches the entire file name.
            suffix: True if final suffix (as determined by pathlib.Path) is
                    matched by any of the passed str.
        """
        self.exclude = tuple(re.compile(i) for i in exclude)

        self.endswith = tuple(endswith)
        self.name = tuple(re.compile(i) for i in name)
        self.suffix = tuple(suffix)

    def matches(self, path: str | Path) -> bool:
        """Returns true if the path matches any filter but not an exclude.

        If no positive filters are specified, any paths that do not match a
        negative filter are considered to match.

        If 'path' is a Path object it is rendered as a posix path (i.e.
        using "/" as the path seperator) before testing with 'exclude' and
        'endswith'.
        """

        posix_path = path.as_posix() if isinstance(path, Path) else path
        if any(bool(exp.search(posix_path)) for exp in self.exclude):
            return False

        # If there are no positive filters set, accept all paths.
        no_filters = not self.endswith and not self.name and not self.suffix

        path_obj = Path(path)
        return (
            no_filters
            or path_obj.suffix in self.suffix
            or any(regex.fullmatch(path_obj.name) for regex in self.name)
            or any(posix_path.endswith(end) for end in self.endswith)
        )

    def filter(self, paths: Sequence[str | Path]) -> Sequence[Path]:
        return [Path(x) for x in paths if self.matches(x)]
