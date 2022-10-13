# Copyright 2022 The Pigweed Authors
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
"""pw_ide settings."""

from inspect import cleandoc
import os
from pathlib import Path
from typing import Any, cast, Dict, List, Optional, Union

from pw_console.yaml_config_loader_mixin import YamlConfigLoaderMixin

PW_IDE_DIR_NAME = '.pw_ide'

_PW_IDE_DEFAULT_DIR = Path(
    os.path.expandvars('$PW_PROJECT_ROOT')) / PW_IDE_DIR_NAME

PW_PIGWEED_CIPD_INSTALL_DIR = Path(
    os.path.expandvars('$PW_PIGWEED_CIPD_INSTALL_DIR'))

PW_ARM_CIPD_INSTALL_DIR = Path(os.path.expandvars('$PW_ARM_CIPD_INSTALL_DIR'))

_DEFAULT_CONFIG = {
    'clangd_additional_query_drivers': [],
    'setup': [],
    'targets': [],
    'working_dir': _PW_IDE_DEFAULT_DIR,
}

_DEFAULT_PROJECT_FILE = Path('$PW_PROJECT_ROOT/.pw_ide.yaml')
_DEFAULT_PROJECT_USER_FILE = Path('$PW_PROJECT_ROOT/.pw_ide.user.yaml')
_DEFAULT_USER_FILE = Path('$HOME/.pw_ide.yaml')


class IdeSettings(YamlConfigLoaderMixin):
    """Pigweed IDE features settings storage class."""
    def __init__(
        self,
        project_file: Union[Path, bool] = _DEFAULT_PROJECT_FILE,
        project_user_file: Union[Path, bool] = _DEFAULT_PROJECT_USER_FILE,
        user_file: Union[Path, bool] = _DEFAULT_USER_FILE,
        default_config: Optional[Dict[str, Any]] = None,
    ) -> None:
        self.config_init(
            config_section_title='pw_ide',
            project_file=project_file,
            project_user_file=project_user_file,
            user_file=user_file,
            default_config=_DEFAULT_CONFIG
            if default_config is None else default_config,
            environment_var='PW_IDE_CONFIG_FILE',
        )

    @property
    def working_dir(self) -> Path:
        """Path to the ``pw_ide`` working directory.

        The working directory holds C++ compilation databases and caches, and
        other supporting files. This should not be a directory that's regularly
        deleted or manipulated by other processes (e.g. the GN ``out``
        directory) nor should it be committed to source control.
        """
        return Path(self._config.get('working_dir', _PW_IDE_DEFAULT_DIR))

    @property
    def targets(self) -> List[str]:
        """The list of targets that should be enabled for code analysis.

        In this case, "target" is analogous to a GN target, i.e., a particular
        build configuration. By default, all available targets are enabled. By
        adding targets to this list, you can constrain the targets that are
        enabled for code analysis to a subset of those that are available, which
        may be useful if your project has many similar targets that are
        redundant from a code analysis perspective.

        Target names need to match the name of the directory that holds the
        build system artifacts for the target. For example, GN outputs build
        artifacts for the ``pw_strict_host_clang_debug`` target in a directory
        with that name in its output directory. So that becomes the canonical
        name for the target.
        """
        return self._config.get('targets', list())

    @property
    def setup(self) -> List[str]:
        """A sequence of commands to automate IDE features setup.

        ``pw ide setup`` should do everything necessary to get the project from
        a fresh checkout to a working default IDE experience. This defines the
        list of commands that makes that happen, which will be executed
        sequentially in subprocesses. These commands should be idempotent, so
        that the user can run them at any time to update their IDE features
        configuration without the risk of putting those features in a bad or
        unexpected state.
        """
        return self._config.get('setup', list())

    @property
    def clangd_additional_query_drivers(self) -> List[str]:
        """Additional query driver paths that clangd should use.

        By default, ``pw_ide`` supplies driver paths for the toolchains included
        in Pigweed. If you are using toolchains that are not supplied by
        Pigweed, you should include path globs to your toolchains here. These
        paths will be given higher priority than the Pigweed toolchain paths.
        """
        return self._config.get('clangd_additional_query_drivers', list())

    def clangd_query_drivers(self) -> List[str]:
        return [
            *[str(Path(p)) for p in self.clangd_additional_query_drivers],
            str(PW_PIGWEED_CIPD_INSTALL_DIR / 'bin' / '*'),
            str(PW_ARM_CIPD_INSTALL_DIR / 'bin' / '*'),
        ]

    def clangd_query_driver_str(self) -> str:
        return ','.join(self.clangd_query_drivers())


def _docstring_set_default(obj: Any,
                           default: Any,
                           literal: bool = False) -> None:
    """Add a default value annotation to a docstring.

    Formatting isn't allowed in docstrings, so by default we can't inject
    variables that we would like to appear in the documentation, like the
    default value of a property. But we can use this function to add it
    separately.
    """
    if obj.__doc__ is not None:
        default = str(default)

        if literal:
            lines = default.splitlines()

            if len(lines) == 0:
                return
            if len(lines) == 1:
                default = f'Default: ``{lines[0]}``'
            else:
                default = ('Default:\n\n.. code-block::\n\n  ' +
                           '\n  '.join(lines))

        doc = cast(str, obj.__doc__)
        obj.__doc__ = f'{cleandoc(doc)}\n\n{default}'


_docstring_set_default(IdeSettings.working_dir, PW_IDE_DIR_NAME, literal=True)
_docstring_set_default(IdeSettings.targets,
                       _DEFAULT_CONFIG['targets'],
                       literal=True)
_docstring_set_default(IdeSettings.setup,
                       _DEFAULT_CONFIG['setup'],
                       literal=True)
_docstring_set_default(IdeSettings.clangd_additional_query_drivers,
                       _DEFAULT_CONFIG['clangd_additional_query_drivers'],
                       literal=True)
