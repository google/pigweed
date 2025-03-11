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
"""Code formatter plugins for Python."""

import os
from pathlib import Path
from typing import Iterable, Iterator, Mapping, Sequence, Tuple

from pw_cli.file_filter import FileFilter
from pw_config_loader import find_config
from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)


DEFAULT_PYTHON_FILE_PATTERNS = FileFilter(endswith=['.py'])

# Pigweed prescribes a .black.toml naming pattern for black configs.
_PIGWEED_BLACK_CONFIG_PATTERN = '.black.toml'


class BlackFormatter(FileFormatter):
    """A formatter that runs ``black`` on files."""

    def __init__(self, config_file: Path | bool = True, **kwargs):
        """Creates a formatter for Python that uses black.

        Args:
            config_file: The black config file to use to configure black. This
                defaults to ``True``, which formats files using the nearest
                ``.black.toml`` file in the parent directory of the file being
                formatted. ``False`` disables this behavior entirely.
        """
        kwargs.setdefault('mnemonic', 'Python (black)')
        kwargs.setdefault('file_patterns', DEFAULT_PYTHON_FILE_PATTERNS)
        super().__init__(**kwargs)
        self._config_file_override: Path | None = (
            config_file if isinstance(config_file, Path) else None
        )
        self._enable_config_lookup = config_file is True

    def _config_file_for(self, file_path: Path) -> Path | None:
        if self._config_file_override:
            return self._config_file_override
        if not self._enable_config_lookup:
            return None

        # Search for Pigweed's `.black.toml`
        configs = find_config.configs_in_parents(
            _PIGWEED_BLACK_CONFIG_PATTERN, file_path
        )
        return next(configs, None)

    def _config_file_args(self, file_path: Path) -> Sequence[str]:
        config = self._config_file_for(file_path)
        if config:
            return ('--config', str(config))

        return ()

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Uses ``black`` to check the formatting of the requested file.

        The file at ``file_path`` is NOT modified by this check.

        Returns:
            A populated
            :py:class:`pw_presubmit.format.core.FormattedFileContents` that
            contains either the result of formatting the file, or an error
            message.
        """
        proc = self.run_tool(
            'black',
            [*self._config_file_args(file_path), '-q', '-'],
            input=file_contents,
        )
        ok = proc.returncode == 0
        formatted_file_contents = proc.stdout if ok else b''

        # On Windows, Black's stdout always has CRLF line endings.
        if os.name == 'nt':
            formatted_file_contents = formatted_file_contents.replace(
                b'\r\n', b'\n'
            )

        return FormattedFileContents(
            ok=ok,
            formatted_file_contents=formatted_file_contents,
            error_message=None if ok else proc.stderr.decode(),
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place using ``black``.

        Returns:
            A FormatFixStatus that contains relevant errors/warnings.
        """
        proc = self.run_tool(
            'black',
            [*self._config_file_args(file_path), '-q', file_path],
        )
        ok = proc.returncode == 0
        return FormatFixStatus(
            ok=ok,
            error_message=None if ok else proc.stderr.decode(),
        )

    def format_files(
        self, paths: Iterable[Path], keep_warnings: bool = True
    ) -> Iterator[Tuple[Path, FormatFixStatus]]:
        """Uses ``black`` to format the specified files in-place.

        Returns:
            An iterator of ``Path`` and
            :py:class:`pw_presubmit.format.core.FormatFixStatus` pairs for each
            file that was not successfully formatted. If ``keep_warnings`` is
            ``True``, any successful format operations with warnings will also
            be returned.
        """
        paths_by_config: Mapping[Path | None, Iterable[Path]] = {}
        if self._config_file_override:
            paths_by_config = {self._config_file_override: paths}
        elif self._enable_config_lookup:
            paths_by_config = find_config.paths_by_nearest_config(
                _PIGWEED_BLACK_CONFIG_PATTERN, paths
            )
        else:
            paths_by_config = {None: paths}

        for config, group_paths in paths_by_config.items():
            config_file_args = ('--config', str(config)) if config else ()
            proc = self.run_tool(
                'black',
                [*config_file_args, '-q', *group_paths],
            )

            # If there's an error, fall back to per-file formatting to figure
            # out which file has problems.
            if proc.returncode != 0:
                yield from super().format_files(group_paths, keep_warnings)
