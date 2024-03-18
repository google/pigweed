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
"""Utilities for manipulating GN configs."""

from __future__ import annotations

from collections import deque
from json import loads as json_loads, dumps as json_dumps
from typing import Any, Deque, Iterable, Iterator, Optional, Set

from pw_build.gn_utils import GnLabel, MalformedGnError

GN_CONFIG_FLAGS = [
    'asmflags',
    'cflags',
    'cflags_c',
    'cflags_cc',
    'cflags_objc',
    'cflags_objcc',
    'defines',
    'include_dirs',
    'inputs',
    'ldflags',
    'lib_dirs',
    'libs',
    'precompiled_header',
    'precompiled_source',
    'rustenv',
    'rustflags',
    'swiftflags',
    'testonly',
]

_INTERNAL_FLAGS = ['nested', 'public_defines']


def _get_prefix(flag: str) -> str:
    """Returns the prefix used to identify values for a particular flag.

    Combining all values in a single set allows methods like `exact_cover` to
    analyze configs for patterns, but the values also need to be able to be
    separated out again. This prefix pattern is chosen as it is guaranteed not
    to be a valid source-relative path or label in GN or Bazel. It is
    encapsulated into its own function to encourage consistency and
    maintainability.

    Args:
        flag: The flag to convert to a prefix.
    """
    return f'{flag}::'


class GnConfig:
    """Represents a GN config.

    Attributes:
        label: The GN label of this config.
        values: A set of config values, prefixed by type. For example, a C
            compiler flag might be 'cflag::-foo'.
    """

    def __init__(
        self, public: bool = False, json: Optional[str] = None
    ) -> None:
        """Create a GN config object.

        Args:
            public: Indicates if this is a `public_config`.
            json: If provided, populates this object from a JSON string.
        """
        self.label: Optional[GnLabel] = None
        self.values: Set[str] = set()
        self._public: bool = public
        self._usages: int = 0
        if json:
            self._from_json(json)

    def __lt__(self, other: GnConfig) -> bool:
        """Compares two configs.

        A config is said to be "less than" another if it comes before the other
        when ordered according to the following rules, evaluated in order:
            * Public configs come before regular configs.
            * More frequently used configs come before those used less.
            * Shorter configs (in terms of values) come before longer ones.
            * Configs whose label comes before the other's comes first.
            * If all else fails, the configs are converted to strings and
              compared lexicographically.
        """
        if self._public != other.public():
            return not self._public and other.public()
        if self._usages != other._usages:
            return self._usages < other._usages
        if len(self.values) != len(other.values):
            return len(self.values) < len(other.values)
        if self.label != other.label:
            return str(self.label) < str(other.label)
        return str(self) < str(other)

    def __eq__(self, other) -> bool:
        return (
            isinstance(other, GnConfig)
            and self._public == other.public()
            and self._usages == other._usages
            and self.values == other.values
            and self.label == other.label
        )

    def __hash__(self) -> int:
        return hash((self._public, self._usages, str(self)))

    def __str__(self) -> str:
        return self.to_json()

    def __bool__(self) -> bool:
        return bool(self.values)

    def _from_json(self, data: str) -> None:
        """Populates this config from a JSON string.

        Args:
            data: A JSON representation of a config.
        """
        obj = json_loads(data)
        if 'label' in obj:
            self.label = GnLabel(obj['label'])
        for flag in GN_CONFIG_FLAGS + _INTERNAL_FLAGS:
            if flag in obj:
                self.add(flag, *obj[flag])
        if 'public' in obj:
            self._public = bool(obj['public'])
        if 'usages' in obj:
            self._usages = int(obj['usages'])

    def to_json(self) -> str:
        """Returns a JSON representation of this config."""
        obj: dict[str, Any] = {}
        if self.label:
            obj['label'] = str(self.label)
        for flag in GN_CONFIG_FLAGS + _INTERNAL_FLAGS:
            if self.has(flag):
                obj[flag] = list(self.get(flag))
        if self._public:
            obj['public'] = self._public
        if self._usages:
            obj['usages'] = self._usages
        return json_dumps(obj)

    def has(self, flag: str) -> bool:
        """Returns whether this config has values for the given flag.

        Args:
            flag: The flag to check for.
        """
        return any(v.startswith(_get_prefix(flag)) for v in self.values)

    def add(self, flag: str, *values: str) -> None:
        """Adds a value to this config for the given flag.

        Args:
            flag: The flag to add values for.
        Variable Args:
            values: Strings to associate with the given flag.
        """
        if flag not in GN_CONFIG_FLAGS and flag not in _INTERNAL_FLAGS:
            raise MalformedGnError(f'invalid flag: {flag}')
        self.values |= {f'{_get_prefix(flag)}{v}' for v in values}

    def get(self, flag: str) -> Iterator[str]:
        """Iterates over the values for the given flag.

        Args:
            flag: the flag to look up.
        """
        prefix = _get_prefix(flag)
        for value in self.values:
            if value.startswith(prefix):
                yield value[len(prefix) :]

    def take(self, flag: str) -> Iterable[str]:
        """Extracts and returns the set of values for the given flag.

        Args:
            flag: The flag to remove and return values for.
        """
        prefix = _get_prefix(flag)
        taken = {v for v in self.values if v.startswith(prefix)}
        self.values = self.values - taken
        return [v[len(prefix) :] for v in taken]

    def deduplicate(self, *configs: GnConfig) -> Iterator[GnConfig]:
        """Removes values found in the given configs.

        Values are only removed if all of the values in a config are present.
        Returns the configs which resulte in values being removed.

        Variable Args:
            configs: The configs whose values should be removed from this config

        Raises:
            ValueError if any of the given configs do not have a label.
        """
        matching = []
        for config in configs:
            if not config.label:
                raise ValueError('config has no label')
            if not config:
                continue
            if config.values <= self.values:
                matching.append(config)
        matching.sort(key=lambda config: len(config.values), reverse=True)
        for config in matching:
            if config.values & self.values:
                self.values = self.values - config.values
                yield config

    def within(self, config: GnConfig) -> bool:
        """Returns whether the values of this config are a subset of another.

        Args:
            config: The config whose values are checked.
        """
        return self.values <= config.values

    def count_usages(self, configs: Iterable[GnConfig]) -> int:
        """Counts how many other configs this config is within.

        Args:
            configs: The set of configs which may contain this object's values.
        """
        self._usages = sum(map(self.within, configs))
        return self._usages

    def public(self) -> bool:
        """Returns whether this object represents a public config."""
        return self._public

    def extract_public(self) -> GnConfig:
        """Extracts and returns the set of values that need to be public.

        'include_dirs' and public 'defines' for a GN target need to be forwarded
        to anything that depends on that target.
        """
        public = GnConfig(public=True)
        public.add('include_dirs', *(self.take('include_dirs')))
        public.add('defines', *(self.take('public_defines')))
        return public

    def generate_label(self, label: GnLabel, index: int) -> None:
        """Creates a label for this config."""
        name = label.name().replace('-', '_')
        name = f'{name}_' or ''
        public = 'public_' if self._public else ''
        self.label = GnLabel(f'{label.dir()}:{name}{public}config{index}')


def _exact_cover(*configs: GnConfig) -> Iterator[GnConfig]:
    """Returns the exact covering set of configs for a given set of configs.

    An exact cover of a sequence of sets is the smallest set of subsets such
    that each element in the union of sets appears in exactly one subset. In
    other words, the subsets are disjoint and every set in the original sequence
    equals some union of subsets.

    As a side effect, this also separates public and regular flags, as GN
    targets have separate lists for `public_configs` and `configs`.

    Variables Args:
        configs: The set of configs to produce an exact cover for.
    """
    pending: Deque[Set[str]] = deque([config.values for config in configs])
    intermediate: Deque[Set[str]] = deque()
    disjoint: Deque[Set[str]] = deque()
    while pending:
        config_a = pending.popleft()
        if not config_a:
            continue
        ok = True
        while disjoint:
            intermediate.append(disjoint.popleft())
        while intermediate:
            config_b = intermediate.popleft()
            ok = False
            if config_a == config_b:
                disjoint.append(config_b)
                break
            if config_a < config_b:
                pending.append(config_b - config_a)
                disjoint.append(config_a)
                break
            if config_a > config_b:
                pending.append(config_a - config_b)
                disjoint.append(config_b)
                break
            config_c = config_a & config_b
            if config_c:
                pending.append(config_a - config_c)
                pending.append(config_b - config_c)
                pending.append(config_c)
                break
            ok = True
            disjoint.append(config_b)
        if ok:
            disjoint.append(config_a)

    for values in disjoint:
        config = GnConfig()
        config.values = values
        public = config.extract_public()
        if public:
            yield public
        if config:
            yield config


def _filter_by_usage(
    covers: Iterator[GnConfig], extract_public: bool, *configs: GnConfig
) -> Iterable[GnConfig]:
    """Filters configs to only include public or those used at least 3 times.

    Args:
        covers: A set of configs that is an exact cover for `configs`.
        extract_public: If true, all public configs are yielded.

    Variable Args:
        configs: A set of configs being conslidated.
    """
    for cover in covers:
        if cover.count_usages(configs) > 2 or (
            extract_public and cover.public()
        ):
            yield cover


def consolidate_configs(
    label: GnLabel, *configs: GnConfig, **kwargs
) -> Iterator[GnConfig]:
    """Extracts and returns the most common sub-configs across a set of configs.

    See also `_exact_cover`. An exact cover of configs can be used to find the
    most common sub-configs. These sub-configs are given labels, and then
    replaced in the original configs.

    Callers may optionally set the keyword argument of `extract_public` to
    `True` if all public configs should be extracted, regardless of usage count.
    Flags like `include_dirs` must be in a GN `public_config` to be forwarded,
    so this is useful for the first consolidation of configs corresponding to a
    Bazel package. Subsequent consolidations, i.e. for a group of Bazel
    packages, may want to avoid pulling public configs out of packages and omit
    this parameter.

    Args:
        label: The base label to use for generated configs.

    Variable Args:
        configs: Configs to examine for common, interesting shared sub-configs.

    Keyword Args:
        extract_public: If true, always considers public configs "interesting".
    """
    extract_public = kwargs.get('extract_public', False)
    covers = list(
        _filter_by_usage(_exact_cover(*configs), extract_public, *configs)
    )
    covers.sort(reverse=True)
    public_i = 1
    config_j = 1
    for cover in covers:
        if cover.public():
            cover.generate_label(label, public_i)
            public_i += 1
        else:
            cover.generate_label(label, config_j)
            config_j += 1
        yield cover
