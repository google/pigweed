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

from json import loads as json_loads, dumps as json_dumps
from pathlib import PurePosixPath
from typing import Optional, Set, Union

from pw_build.bazel_query import BazelRule
from pw_build.gn_config import GnConfig, GN_CONFIG_FLAGS
from pw_build.gn_utils import (
    GnLabel,
    GnPath,
    GnVisibility,
    MalformedGnError,
)


class GnTarget:  # pylint: disable=too-many-instance-attributes
    """Represents a Pigweed Build target.

    Attributes:
        config: Variables on the target that can be added to a config, e.g.
            those in `pw_build.gn_config.GN_CONFIG_FLAGS.

    The following attributes match those of `pw_internal_build_target` in
    //pw_build/gn_internal/build_target.gni
        remove_configs

    The following attributes are analogous to GN variables, see
    https://gn.googlesource.com/gn/+/main/docs/reference.md#target_variables:
        visibility
        testonly
        public
        sources
        inputs
        public_configs
        configs
        public_deps
        deps
    """

    def __init__(
        self,
        base_label: Union[str, PurePosixPath, GnLabel],
        base_path: Union[str, PurePosixPath, GnPath],
        bazel: Optional[BazelRule] = None,
        json: Optional[str] = None,
        check_includes: bool = True,
    ) -> None:
        """Creates a GN target

        Args:

        """
        self._type: str
        self._label: GnLabel
        self._base_label: GnLabel = GnLabel(base_label)
        self._base_path: GnPath = GnPath(base_path)
        self._repos: Set[str] = set()
        self.visibility: list[GnVisibility] = []
        self.testonly: bool = False
        self.check_includes = check_includes
        self.public: list[GnPath] = []
        self.sources: list[GnPath] = []
        self.inputs: list[GnPath] = []
        self.config: GnConfig = GnConfig()
        self.public_configs: Set[GnLabel] = set()
        self.configs: Set[GnLabel] = set()
        self.remove_configs: Set[GnLabel] = set()
        self.public_deps: Set[GnLabel] = set()
        self.deps: Set[GnLabel] = set()
        if bazel:
            self._from_bazel(bazel)
        elif json:
            self._from_json(json)
        else:
            self._label = GnLabel(base_label)

    def _from_bazel(self, rule: BazelRule) -> None:
        """Populates target info from a Bazel Rule.

        Filenames will be relative to the given path to the source directory.

        Args:
            rule: The Bazel rule to populate this object with.
        """
        kind = rule.kind()
        linkstatic = rule.get_bool('linkstatic')
        visibility = rule.get_list('visibility')

        self.testonly = rule.get_bool('testonly')
        public = rule.get_list('hdrs')
        sources = rule.get_list('srcs')
        inputs = rule.get_list('additional_linker_inputs')

        include_dirs = rule.get_list('includes')
        self.config.add('public_defines', *rule.get_list('defines'))
        self.config.add('cflags', *rule.get_list('copts'))
        self.config.add('ldflags', *rule.get_list('linkopts'))
        self.config.add('defines', *rule.get_list('local_defines'))

        public_deps = rule.get_list('deps')
        deps = rule.get_list('implementation_deps')

        rule_label = rule.label()
        if rule_label.startswith('//'):
            rule_label = rule_label[2:]
        self._label = self._base_label.joinlabel(rule_label)

        # Translate Bazel kind to GN target type.
        if kind == 'cc_library':
            if linkstatic:
                self._type = 'pw_static_library'
            else:
                self._type = 'pw_source_set'
        else:
            raise MalformedGnError(f'unsupported Bazel kind: {kind}')

        # Bazel always implicitly includes private visibility.
        visibility.append('//visibility:private')
        for scope in visibility:
            self.add_visibility(bazel=scope)

        # Add includer directories. Bazel implicitly includes the project root.
        include_dirs.append('//')
        gn_paths = [
            GnPath(self._base_path, bazel=file) for file in include_dirs
        ]
        self.config.add('include_dirs', *[str(gn_path) for gn_path in gn_paths])

        for path in public:
            self.add_path('public', bazel=path)
        for path in sources:
            self.add_path('sources', bazel=path)
        for path in inputs:
            self.add_path('inputs', bazel=path)
        for label in public_deps:
            self.add_dep(public=True, bazel=label)
        for label in deps:
            self.add_dep(bazel=label)

    def _from_json(self, data: str) -> None:
        """Populates this target from a JSON string.

        Args:
            data: The JSON data to populate this object with.
        """
        obj = json_loads(data)
        self._type = obj.get('target_type')
        name = obj.get('target_name')
        package = obj.get('package', '')
        self._label = GnLabel(
            self._base_label.joinlabel(package), gn=f':{name}'
        )
        self.visibility = [
            GnVisibility(self._base_label, self._label, gn=scope)
            for scope in obj.get('visibility', [])
        ]
        self.testonly = bool(obj.get('testonly', False))
        self.testonly = bool(obj.get('check_includes', False))
        self.public = [GnPath(path) for path in obj.get('public', [])]
        self.sources = [GnPath(path) for path in obj.get('sources', [])]
        self.inputs = [GnPath(path) for path in obj.get('inputs', [])]
        for flag in GN_CONFIG_FLAGS:
            if flag in obj:
                self.config.add(flag, *obj[flag])
        self.public_configs = {
            GnLabel(label) for label in obj.get('public_configs', [])
        }
        self.configs = {GnLabel(label) for label in obj.get('configs', [])}
        self.public_deps = {
            GnLabel(label) for label in obj.get('public_deps', [])
        }
        self.deps = {GnLabel(label) for label in obj.get('deps', [])}

    def to_json(self) -> str:
        """Returns a JSON representation of this target."""
        obj: dict[str, Union[bool, str, list[str]]] = {}
        if self._type:
            obj['target_type'] = self._type
        obj['target_name'] = self.name()
        obj['package'] = self.package()
        if self.visibility:
            obj['visibility'] = [str(scope) for scope in self.visibility]
        if self.testonly:
            obj['testonly'] = self.testonly
        if not self.check_includes:
            obj['check_includes'] = self.check_includes
        if self.public:
            obj['public'] = [str(path) for path in self.public]
        if self.sources:
            obj['sources'] = [str(path) for path in self.sources]
        if self.inputs:
            obj['inputs'] = [str(path) for path in self.inputs]
        for flag in GN_CONFIG_FLAGS:
            if self.config.has(flag):
                obj[flag] = list(self.config.get(flag))
        if self.public_configs:
            obj['public_configs'] = [
                str(label) for label in self.public_configs
            ]
        if self.configs:
            obj['configs'] = [str(label) for label in self.configs]
        if self.remove_configs:
            obj['remove_configs'] = [
                str(label) for label in self.remove_configs
            ]
        if self.public_deps:
            obj['public_deps'] = [str(label) for label in self.public_deps]
        if self.deps:
            obj['deps'] = [str(label) for label in self.deps]
        return json_dumps(obj)

    def name(self) -> str:
        """Returns the target name."""
        return self._label.name()

    def label(self) -> GnLabel:
        """Returns the target label."""
        return self._label

    def type(self) -> str:
        """Returns the target type."""
        return self._type

    def repos(self) -> Set[str]:
        """Returns any external repositories referenced from Bazel rules."""
        return self._repos

    def package(self) -> str:
        """Returns the relative path from the base label."""
        pkgname = self._label.relative_to(self._base_label).split(':')[0]
        return '' if pkgname == '.' else pkgname

    def add_path(self, dst: str, **kwargs) -> None:
        """Adds a GN path using this target's source root.

        Args:
            dst: Variable to add the path to. Should be one of 'public',
                'sources', or 'inputs'.

        Keyword Args:
            Same as `GnPath.__init__`.
        """
        getattr(self, dst).append(GnPath(self._base_path, **kwargs))

    def add_config(self, label: GnLabel, public: bool = False) -> None:
        """Adds a GN config to this target.

        Args:
            label: The label of the config to add to this target.
            public: If true, the config is added as a `public_config`.
        """
        self.remove_configs.discard(label)
        if public:
            self.public_configs.add(label)
            self.configs.discard(label)
        else:
            self.public_configs.discard(label)
            self.configs.add(label)

    def remove_config(self, label: GnLabel) -> None:
        """Adds the given config to the list of default configs to remove.

        Args:
            label: The config to add to `remove_configs`.
        """
        self.public_configs.discard(label)
        self.configs.discard(label)
        self.remove_configs.add(label)

    def add_dep(self, **kwargs) -> None:
        """Adds a GN dependency to the target using this target's base label.

        Keyword Args:
            Same as `GnLabel.__init__`.
        """
        dep = GnLabel(self._base_label, **kwargs)
        repo = dep.repo()
        if repo:
            self._repos.add(repo)
        if dep.public():
            self.public_deps.add(dep)
        else:
            self.deps.add(dep)

    def add_visibility(self, **kwargs) -> None:
        """Adds a GN visibility scope using this target's base label.

        This removes redundant scopes:
        * If the scope to be added is already within an existing scope, it is
          ignored.
        * If existing scopes are within the scope being added, they are removed.

        Keyword Args:
            Same as `GnVisibility.__init__`.
        """
        new_scope = GnVisibility(self._base_label, self._label, **kwargs)
        if any(new_scope.within(scope) for scope in self.visibility):
            return
        self.visibility = list(
            filter(lambda scope: not scope.within(new_scope), self.visibility)
        )
        self.visibility.append(new_scope)

    def make_relative(self, item: Union[GnLabel, GnVisibility]) -> str:
        """Returns a label relative to this target.

        Args:
            item: The GN item to rebase relative to this target.
        """
        return item.relative_to(self._label)
