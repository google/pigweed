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
"""Generates a BUILD.gn file from `cc_library` rules in Bazel workspace."""

import argparse
import json
import os
import subprocess
from collections import defaultdict
from pathlib import Path
from string import Template
from typing import Dict, List, IO, Iterable, Iterator, Set

from pw_build.bazel_query import BazelWorkspace
from pw_build.gn_config import consolidate_configs, GnConfig
from pw_build.gn_target import GnTarget
from pw_build.gn_utils import GnLabel, GnPath
from pw_build.gn_writer import GnFile, GnWriter, MalformedGnError

_DOCS_RST_TEMPLATE = Template(
    '''
.. _module-pw_third_party_$repo:

$name_section
$name
$name_section
The ``$$dir_pw_third_party/$repo/`` module provides build files to allow
optionally including upstream $name.

---------------$name_subsection
Using upstream $name
---------------$name_subsection
If you want to use $name, you must do the following:

Submodule
=========
Add $name to your workspace with the following command.

.. code-block:: sh

  git submodule add $url \\
    third_party/$repo/src

GN
==
* Set the GN var ``dir_pw_third_party_$repo`` to the location of the
  $name source.

  If you used the command above, this will be
  ``//third_party/$repo/src``

  This can be set in your args.gn or .gn file like:
  ``dir_pw_third_party_$repo_var = "//third_party/$repo/src"``

Updating
========
The GN build files are generated from the third-party Bazel build files using
$$dir_pw_build/py/pw_build/$script.

The script uses data taken from ``$$dir_pw_third_party/$repo/repo.json``.
The schema of ``repo.json`` is described in :ref:`module-pw_build-third-party`.

The script should be re-run whenever the submodule is updated or the JSON file
is modified. Specify the location of the Bazel repository can be specified using
the ``-w`` option, e.g.

.. code-block:: sh

  python pw_build/py/pw_build/$script \\
    -w third_party/$repo/src

.. DO NOT EDIT BELOW THIS LINE. Generated section.
'''.lstrip()
)

_GIT_SHORT_REV_LEN = 8


class GnGenerator:
    """Maintains state while GN files are generated from Bazel files.

    Attributes:
        packages: The set of packages/sub-directories, each of which will have a
            BUILD.gn.
        configs: A mapping of package names to common GN configs for that
            package.
        targets: A mapping of package names to that Gn targets in that package.
    """

    def __init__(self) -> None:
        self.packages: Set[str] = set()
        self._workspace: BazelWorkspace
        self._base_label: GnLabel
        self._base_path: GnPath
        self._repo: str
        self._repo_var: str
        self._repos: Dict[str, Set[str]] = defaultdict(set)
        self._no_gn_check: List[GnLabel] = []
        self.configs: Dict[str, List[GnConfig]] = defaultdict(list)
        self.targets: Dict[str, List[GnTarget]] = defaultdict(list)

        self.packages.add('')
        self.configs[''] = []
        self.targets[''] = []

    def set_repo(self, repo: str) -> None:
        """Sets the repository related variables.

        This does not need to be called unless `load_workspace` is not being
        called (as in some unit tests).

        Args:
            repo: The repository name.
        """
        self._repo = repo
        self._repo_var = repo.replace('-', '_')
        self._base_label = GnLabel(f'$dir_pw_third_party/{repo}')
        self._base_path = GnPath(f'$dir_pw_third_party_{self._repo_var}')

    def exclude_from_gn_check(self, **kwargs) -> None:
        """Mark a target as being excluding from `gn check`.

        This should be called before loading or adding targets.

        Args:
            kwargs: Same as `GnLabel`.
        """
        self._no_gn_check.append(GnLabel(self._base_label, **kwargs))

    def load_workspace(self, workspace_path: Path) -> str:
        """Loads a Bazel workspace.

        Args:
            workspace_path: The path to the Bazel workspace.
        """
        self._workspace = BazelWorkspace(workspace_path)
        self.set_repo(self._workspace.repo)
        return self._repo

    def load_targets(self, kind: str, allow_testonly: bool) -> None:
        """Analyzes a Bazel workspace and loads target info from it.

        Target info will only be added for `kind` rules. Additionally,
        libraries marked "testonly" may be included if `allow_testonly` is
        present and true in the repo.json file.

        Args:
            kind: The Bazel rule kind to parse.
            allow_testonly: If true, include testonly targets as well.
        """
        for rule in self._workspace.get_rules(kind):
            self.add_target(allow_testonly, bazel=rule)

    def add_target(self, allow_testonly: bool = False, **kwargs) -> None:
        """Adds a target using this object's base label and source root.

        Args:
            allow_testonly: If true, include testonly targets as well.

        Keyword Args:
            Same as `GnTarget`.
        """
        target = GnTarget(self._base_label.dir(), self._base_path, **kwargs)
        target.check_includes = target.label() not in self._no_gn_check
        if allow_testonly or not target.testonly:
            package = target.package()
            self.packages.add(package)
            self._repos[package].update(target.repos())
            self.targets[package].append(target)

    def add_configs(self, package: str, *configs: GnConfig) -> None:
        """Adds configs for the given package.

        This is mostly used for testing and debugging.

        Args:
            package: The package to add configs for.

        Variable Args:
            configs: The configs to add.
        """
        self.configs[package].extend(configs)

    def generate_configs(
        self, configs_to_add: Iterable[str], configs_to_remove: Iterable[str]
    ) -> bool:
        """Extracts the most common flags into common configs.

        Args:
            configs_to_add: Additional configs to add to every target.
            configs_to_remove: Default configs to remove from every target.
        """
        added = [GnLabel(label) for label in configs_to_add]
        removed = [GnLabel(label) for label in configs_to_remove]
        all_configs = []
        for targets in self.targets.values():
            for target in targets:
                for label in added:
                    target.add_config(label)
                for label in removed:
                    target.remove_config(label)
                all_configs.append(target.config)

        consolidated = list(consolidate_configs(self._base_label, *all_configs))

        def count_packages(config: GnConfig) -> int:
            return sum(
                [
                    any(config.within(target.config) for target in targets)
                    for _package, targets in self.targets.items()
                ]
            )

        common = list(
            filter(lambda config: count_packages(config) > 1, consolidated)
        )

        self.configs[''] = sorted(common)
        for targets in self.targets.values():
            for target in targets:
                for config in target.config.deduplicate(*common):
                    if not config.label:
                        raise MalformedGnError('config is missing label')
                    target.add_config(config.label, public=config.public())

        for package, targets in self.targets.items():
            configs = [target.config for target in targets]
            common = list(
                consolidate_configs(
                    self._base_label.joinlabel(package),
                    *configs,
                    extract_public=True,
                )
            )
            self.configs[package].extend(common)
            self.configs[package].sort()
            for target in targets:
                for config in target.config.deduplicate(*common):
                    if not config.label:
                        raise MalformedGnError('config is missing label')
                    target.add_config(config.label, public=config.public())
        return True

    def write_repo_gni(self, repo_gni: GnWriter, name: str) -> None:
        """Write the top-level GN import that declares build arguments.

        Args:
            repo_gni: The output writer object.
            name: The third party module_name.
        """
        repo_gni.write_target_start('declare_args')
        repo_gni.write_comment(
            f'If compiling tests with {name}, this variable is set to the path '
            f'to the {name} installation. When set, a pw_source_set for the '
            f'{name} library is created at "$dir_pw_third_party/{self._repo}".'
        )
        repo_gni.write(f'dir_pw_third_party_{self._repo_var} = ""')
        repo_gni.write_end()

    def write_build_gn(self, package: str, build_gn: GnWriter) -> None:
        """Write the target info for a package to a BUILD.gn file.

        Args:
            package: The name of the package to write a BUILD.gn for.
            build_gn: The output writer object.
            no_gn_check: List of targets with `check_includes = false`.
        """
        build_gn.write_imports(['//build_overrides/pigweed.gni'])
        build_gn.write_blank()
        imports = set()
        imports.add('$dir_pw_build/target_types.gni')

        if not package:
            imports.add('$dir_pw_docgen/docs.gni')
        for repo in self._repos[package]:
            repo = build_gn.repos[repo]
            imports.add('$dir_pw_build/error.gni')
            imports.add(f'$dir_pw_third_party/{repo}/{repo}.gni')
        if self._repo:
            imports.add(f'$dir_pw_third_party/{self._repo}/{self._repo}.gni')
        build_gn.write_imports(sorted(list(imports)))

        for config in sorted(self.configs[package], reverse=True):
            build_gn.write_config(config)

        targets = self.targets[package]
        targets.sort(key=lambda target: target.name())
        for target in targets:
            build_gn.write_target(target)

        if not package:
            build_gn.write_target_start('pw_doc_group', 'docs')
            build_gn.write_list('sources', ['docs.rst'])
            build_gn.write_end()

    def write_docs_rst(self, docs_rst: IO, name: str) -> None:
        """Writes new, top-level documentation for the repo.

        Args:
            docs_rst: The output object.
            name: The third party module_name.
        """
        contents = _DOCS_RST_TEMPLATE.substitute(
            script=os.path.basename(__file__),
            name=name,
            name_section='=' * len(name),
            name_subsection='-' * len(name),
            repo=self._repo,
            repo_var=self._repo_var,
            url=self._workspace.url(),
        )
        docs_rst.write('\n'.join(self.update_version(contents.split('\n'))))

    def update_version(self, lines: Iterable[str]) -> Iterator[str]:
        """Replaces the "Version" part of docs.rst with the latest revision.

        This will truncate everything after the "generated section" comment and
        add the comment and version information. If the file does not have the
        comment, the comment and information will appended to the end of the
        file.

        Args:
            lines: Iterator of lines.
        """
        comment = '.. DO NOT EDIT BELOW THIS LINE. Generated section.'
        url = self._workspace.url().rstrip('.git')
        revision = self._workspace.revision()
        short = revision[:_GIT_SHORT_REV_LEN]
        for line in lines:
            if line == comment:
                break
            yield line
        yield comment
        yield ''
        yield 'Version'
        yield '======='
        yield f'The update script was last run for revision `{short}`_.'
        yield ''
        yield f'.. _{short}: {url}/tree/{revision}'
        yield ''


def write_owners(owners: IO) -> None:
    """Write an OWNERS file, but only if it does not already exist.

    Args:
        owners: The output object.
    """
    try:
        result = subprocess.run(
            ['git', 'config', '--get', 'user.email'],
            check=True,
            capture_output=True,
        )
        owners.write(result.stdout.decode('utf-8'))
    except subprocess.CalledProcessError:
        # Couldn't get owner from git config.
        pass


def _parse_args() -> List[Path]:
    """Parse arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'workspace',
        type=Path,
        nargs='+',
        help=('Path to Bazel workspace to be queried.'),
    )
    args = parser.parse_args()
    return args.workspace


def _generate_gn(workspace_path: Path) -> None:
    """Creates GN, doc, and OWNERS files for a third-party repository.

    Args:
        workspace_path: Path to the Bazel workspace.
    """
    pw_root = os.getenv('PW_ROOT')
    if not pw_root:
        raise RuntimeError('PW_ROOT is not set')

    generator = GnGenerator()
    repo = generator.load_workspace(workspace_path)
    output = Path(pw_root, 'third_party', repo)

    with open(output.joinpath('repo.json')) as file:
        obj = json.load(file)
        name = obj['name']
        repos = obj.get('repos', {})
        aliases = obj.get('aliases', {})
        add_configs = obj.get('add', [])
        removeconfigs = obj.get('remove', [])
        allow_testonly = obj.get('allow_testonly', False)
        no_gn_check = obj.get('no_gn_check', [])

    for exclusion in no_gn_check:
        generator.exclude_from_gn_check(bazel=exclusion)
    generator.load_targets('cc_library', allow_testonly)
    generator.generate_configs(add_configs, removeconfigs)

    with GnFile(Path(output, f'{repo}.gni')) as repo_gni:
        generator.write_repo_gni(repo_gni, name)

    for package in generator.packages:
        with GnFile(Path(output, package, 'BUILD.gn'), package) as build_gn:
            build_gn.repos = repos
            build_gn.aliases = aliases
            generator.write_build_gn(package, build_gn)

    try:
        with open(Path(output, 'docs.rst'), 'x') as docs_rst:
            generator.write_docs_rst(docs_rst, name)
    except OSError:
        # docs.rst file already exists, just replace the version section.
        with open(Path(output, 'docs.rst'), 'r') as docs_rst:
            contents = '\n'.join(generator.update_version(docs_rst))
        with open(Path(output, 'docs.rst'), 'w') as docs_rst:
            docs_rst.write(contents)

    try:
        with open(Path(output, 'OWNERS'), 'x') as owners:
            write_owners(owners)
    except OSError:
        pass  # OWNERS file already exists.


if __name__ == '__main__':
    for workspace in _parse_args():
        _generate_gn(workspace)
