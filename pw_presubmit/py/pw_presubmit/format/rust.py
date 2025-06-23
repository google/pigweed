# Copyright 2025 The Pigweed Authors
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
"""Code formatter plugin for Rust."""

from pathlib import Path
from typing import Final, Iterable, Iterator, Mapping, Sequence, Tuple

from pw_cli.file_filter import FileFilter
from pw_config_loader import find_config
from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)


DEFAULT_RUST_FILE_PATTERNS = FileFilter(endswith=['.rs'])
_RUSTFMT_CONFIG_PATTERN = 'rustfmt.toml'


class RustfmtFormatter(FileFormatter):
    """A formatter that runs `rustfmt` on files."""

    DEFAULT_FLAGS: Final[Sequence[str]] = ()

    def __init__(self, tool_flags: Sequence[str] = DEFAULT_FLAGS, **kwargs):
        kwargs.setdefault('mnemonic', 'Rust')
        kwargs.setdefault('file_patterns', DEFAULT_RUST_FILE_PATTERNS)
        super().__init__(**kwargs)
        self.rustfmt_flags = list(tool_flags)

    @staticmethod
    def _config_path_for(file_path: Path) -> Path | None:
        # Search for `rustfmt.toml`
        configs = find_config.configs_in_parents(
            _RUSTFMT_CONFIG_PATTERN, file_path
        )
        return next(configs, None)

    @staticmethod
    def _config_path_args(config_path: Path | None) -> Sequence[str]:
        if config_path:
            return ('--config-path', str(config_path))
        return ()

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Uses ``rustfmt`` to check the formatting of the requested file.

        The file at ``file_path`` is NOT modified by this check.

        Returns:
            A populated
            :py:class:`pw_presubmit.format.core.FormattedFileContents` that
            contains either the result of formatting the file, or an error
            message.
        """
        config = self._config_path_for(file_path)
        proc = self.run_tool(
            'rustfmt',
            [*self._config_path_args(config), *self.rustfmt_flags],
            input=file_contents,
        )
        ok = proc.returncode == 0
        return FormattedFileContents(
            ok=ok,
            formatted_file_contents=proc.stdout,
            error_message=None if ok else proc.stderr.decode(),
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place using ``rustfmt``.

        Returns:
            A FormatFixStatus that contains relevant errors/warnings.
        """
        # When running on a single file, rustfmt will find the config file
        # automatically. No need to specify --config-path.
        proc = self.run_tool(
            'rustfmt',
            [*self.rustfmt_flags, str(file_path)],
        )
        ok = proc.returncode == 0
        return FormatFixStatus(
            ok=ok,
            error_message=None if ok else proc.stderr.decode(),
        )

    def format_files(
        self, paths: Iterable[Path], keep_warnings: bool = True
    ) -> Iterator[Tuple[Path, FormatFixStatus]]:
        """Uses ``rustfmt`` to format the specified files in-place.

        Returns:
            An iterator of ``Path`` and
            :py:class:`pw_presubmit.format.core.FormatFixStatus` pairs for each
            file that was not successfully formatted. If ``keep_warnings`` is
            ``True``, any successful format operations with warnings will also
            be returned.
        """
        paths_by_config: Mapping[
            Path | None, Iterable[Path]
        ] = find_config.paths_by_nearest_config(_RUSTFMT_CONFIG_PATTERN, paths)

        for _, group_paths in paths_by_config.items():
            str_paths = [str(p) for p in group_paths]
            proc = self.run_tool(
                'rustfmt',
                [*self.rustfmt_flags, *str_paths],
            )

            # If there's an error, fall back to per-file formatting to figure
            # out which file has problems.
            if proc.returncode != 0:
                yield from super().format_files(group_paths, keep_warnings)
