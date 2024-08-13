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
"""Helpers that use wildcards to match one or more directories."""

def match_dir_internal(include, exclude = [], allow_empty = True, _fail_callback = fail):
    """The actual implementation of match_dir with more testability.

    DO NOT USE THIS DIRECTLY!!! Use `match_dir()`!

    Args:
      include: A list of wildcard patterns to match against.
      exclude: A list of wildcard patterns to exclude.
      allow_empty: Whether or not to permit returning `None`.
      _fail_callback: Callback to call when an error is encountered.

    Returns:
      Path to a single directory that matches the specified constraints.
    """
    matches = glob_dirs(
        include,
        exclude,
        allow_empty = allow_empty,
    )
    if not allow_empty and not matches:
        return _fail_callback(
            ("glob pattern {} didn't match anything, but allow_empty is set " +
             "to False").format(include),
        )
    if len(matches) > 1:
        return _fail_callback(
            ("glob pattern {} matches multiple directories when only one " +
             "was requested: {}").format(include, matches),
        )
    return matches[0] if matches else None

def match_dir(include, exclude = [], allow_empty = True):
    """Identifies a single directory using a wildcard pattern.

    This helper follows the same semantics as Bazel's native glob() function,
    but only matches a single directory. If more than one match is found, this
    will fail.

    Args:
      include: A list of wildcard patterns to match against.
      exclude: A list of wildcard patterns to exclude.
      allow_empty: Whether or not to permit returning `None`.

    Returns:
      Path to a single directory that matches the specified constraints, or
      `None` if no match is found and `allow_empty` is `True`.
    """
    return match_dir_internal(
        include,
        exclude = exclude,
        allow_empty = allow_empty,
    )

def glob_dirs(include, exclude = [], allow_empty = True):
    """Matches the provided glob pattern to identify a list of directories.

    This helper follows the same semantics as Bazel's native glob() function,
    but only matches directories.

    Args:
      include: A list of wildcard patterns to match against.
      exclude: A list of wildcard patterns to exclude.
      allow_empty: Whether or not to permit an empty list of matches.

    Returns:
      List of directory paths that match the specified constraints.
    """
    without_dirs = native.glob(
        include,
        exclude,
        exclude_directories = 1,
        allow_empty = True,
    )
    with_dirs = native.glob(
        include,
        exclude,
        exclude_directories = 0,
        allow_empty = allow_empty,
    )
    results = {p: None for p in with_dirs}
    for p in without_dirs:
        results.pop(p)

    return list(results.keys())
