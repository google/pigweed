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
"""Common function definitions."""

import collections
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

try:
    from pw_build_mcuxpresso.components import Project
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from .components import Project  # type: ignore


@dataclass
class BuildTarget:
    """Representation of single build target.
    Target's attributes can be represented as dictionary in which
    the keys are strings (attribute names) and values are either
    strings, lists, integers or booleans.

    Properties:
        target_type: type of rule to instantiate
        name: name of this target
        attrs: target attributes
    """

    target_type: str
    name: str
    attrs: dict[str, Any] = field(default_factory=dict)

    def label(self, prefix: str = "") -> str:
        """Returns string representation of this target's label."""
        return f"{prefix}:{self.name}"

    def __lt__(self, other) -> bool:
        return self.label() < other.label()

    def __str__(self) -> str:
        return self.label()


def normalize_path_list(targets: list[Path]) -> list[str]:
    """Converts given paths to their string representation
    using '/' as a path separator
    """
    return [target.as_posix() for target in targets]


def path_to_component_id(filepath: Path | str) -> str:
    """Converts filepath to component id by replacing '/' with '.'"""
    if isinstance(filepath, Path):
        filepath = filepath.as_posix()
    return filepath.replace("/", ".")


def find_component_dep_cycles(project: Project) -> list[tuple[str, str]]:
    """Finds dependency cycles between components

    Args:
        projetc: mcuxpresso project
    Returns:
        list of component pairs which depend on one another
    """
    components = project.components

    cycles = list()
    visited = set()
    dependencies: collections.deque = collections.deque()

    for component in components.values():
        visited.add(component.id)

        dependencies.extend(component.dependencies)

        while dependencies:
            dep = components[dependencies.popleft()]
            if component.id in dep.dependencies:
                cycles.append((dep.id, component.id))

            visited.add(dep.id)
            dependencies.extend(
                dep.dependencies.difference(visited.union(dependencies))
            )

    return cycles
