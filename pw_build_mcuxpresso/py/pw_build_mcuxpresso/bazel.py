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
"""Bazel output support."""

from dataclasses import dataclass
from itertools import chain
from pathlib import Path
from typing import Any, Iterable, Iterator

try:
    from pw_build_mcuxpresso.common import (
        BuildTarget,
        find_component_dep_cycles,
        normalize_path_list,
        path_to_component_id,
    )
    from pw_build_mcuxpresso.components import Component, Project
    from pw_build_mcuxpresso.consts import (
        SDK_COMMONS_NAME,
        SDK_DEFAULT_COPTS,
        SDK_USER_CONFIG_NAME,
    )
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from common import (  # type: ignore
        BuildTarget,
        find_component_dep_cycles,
        normalize_path_list,
        path_to_component_id,
    )
    from components import Component, Project  # type: ignore
    from consts import (  # type: ignore
        SDK_COMMONS_NAME,
        SDK_DEFAULT_COPTS,
        SDK_USER_CONFIG_NAME,
    )


@dataclass
class BazelVariable:
    """Representation of Bazel variable"""

    name: str
    value: Any

    def __str__(self) -> str:
        return f"{self.name} = {self.value}"


SHARED_BAZEL_COPTS = BazelVariable("COPTS", SDK_DEFAULT_COPTS)

# pylint: disable=line-too-long
BUILDFILE_HEADER = rf"""### This file was auto generated. Do not edit manually. ###
package(default_visibility = ["//visibility:public"])
{SHARED_BAZEL_COPTS}
"""
# pylint: enable=line-too-long

USER_CONFIG_TARGET = BuildTarget(
    "label_flag",
    SDK_USER_CONFIG_NAME,
    {
        "build_setting_default": "@pigweed//pw_build:empty_cc_library",
    },
)


def _resolve_component_dep_cycles(project: Project) -> dict[str, Component]:
    """Resolves dependency cycles between components

    Args:
        project: mcuxpresso project
    Returns:
        mapping of component ids to components without dependency cycles
    """
    components = project.components.copy()
    for id_a, id_b in find_component_dep_cycles(project):
        a, b = components[id_a], components[id_b]
        # Workaround for bug in serial_manager_uart component from manifest.
        # It has circular dependency with serial_manager component as both
        # serial_manager has dependency on serial_manager_uart and vice versa.
        # Fix this by extracting common dependencies using serial_manager_uart
        # as base component.
        if id_a == "component.serial_manager_uart.MIMXRT595S":
            c = a.merged(b)
        else:
            c = b.merged(a)

        components[c.id] = c

    return components


def headers_cc_library(project: Project) -> BuildTarget:
    """Generates common headers Bazel cc_library target

    Args:
        project: mcuxpresso project
        output_path: path to output directory
    """
    deduplicate_chain = lambda it: sorted(set(chain.from_iterable(it)))

    components = project.components.values()

    defines = deduplicate_chain(component.defines for component in components)

    headers = deduplicate_chain(component.headers for component in components)
    headers = normalize_path_list(headers)

    includes = deduplicate_chain(
        component.include_dirs for component in components
    )
    includes = normalize_path_list(includes)

    return BuildTarget(
        "cc_library",
        SDK_COMMONS_NAME,
        {
            "deps": [USER_CONFIG_TARGET],
            "defines": defines,
            "hdrs": headers,
            "includes": includes,
            "copts": SHARED_BAZEL_COPTS,
        },
    )


def import_targets(libraries: Iterable[Path]) -> list[BuildTarget]:
    """Generates Bazel cc_import targets for static libraries

    Args:
        libraries: paths to '.a' library files
    """

    return sorted(
        (
            BuildTarget(
                "cc_import",
                path_to_component_id(library),
                {
                    "static_library": library.as_posix(),
                },
            )
            for library in libraries
        ),
    )


def component_targets(
    components: dict[str, Component],
    imports: list[BuildTarget],
    commons: BuildTarget,
) -> list[BuildTarget]:
    """Returns Bazel cc_library targets representing SDK components

    Args:
        components: mapping of component ids to SDK components
        imports: list of import targets
        commons: target containing common definitions
    """

    libraries = {target.name: target for target in imports}
    parsed: dict[str, BuildTarget] = dict()

    def component_target(component: Component) -> BuildTarget:
        if component.id in parsed.keys():
            return parsed[component.id]

        libs = [libraries[path_to_component_id(lib)] for lib in component.libs]

        deps = sorted(
            chain(
                [commons],
                libs,
                (
                    component_target(components[dep_id])
                    if dep_id not in parsed.keys()
                    else parsed[dep_id]
                    for dep_id in component.dependencies
                ),
            ),
        )

        sources = normalize_path_list(component.sources)

        attrs: dict[str, Any] = {
            "srcs": sources,
            "deps": deps,
            "copts": SHARED_BAZEL_COPTS,
        }

        if component.private:
            attrs["visibility"] = ["//visibility:private"]

        if component.alwayslink:
            attrs["alwayslink"] = True

        target = BuildTarget("cc_library", component.id, attrs)
        parsed[component.id] = target

        return target

    for component in components.values():
        component_target(component)

    return sorted(parsed.values())


def generate_bazel_targets(project: Project) -> Iterator[BuildTarget]:
    """Generates Bazel targets for a project

    Args:
        project: MCUXpresso project to output
    """
    components = _resolve_component_dep_cycles(project)
    libraries = set(
        chain.from_iterable(component.libs for component in components.values())
    )

    commons = headers_cc_library(project)
    imports = import_targets(libraries)

    return chain(
        [USER_CONFIG_TARGET, commons],
        imports,
        component_targets(components, imports, commons),
    )


def target_to_bazel(target: BuildTarget, indent: int = 4) -> str:
    """Converts BuildTarget to Bazel string representation

    Args:
        target: build target
        indent: specifies amount of indentation added for each nesting level
    """
    text = f"{target.target_type}(\n"
    text += f'{" " * indent}name = "{target.name}",\n'

    for key, value in sorted(target.attrs.items(), key=lambda kv: kv[0]):
        # Handle formatting for specific types
        if isinstance(value, str):
            # Wrap string in quotes and escape inner ones
            value = '"{}"'.format(value.replace('"', r"\""))
        elif isinstance(value, list):
            # Skip printing empty arrays
            if len(value) == 0:
                continue
            for i, v in enumerate(value):
                if isinstance(v, BuildTarget):
                    # Print target's label
                    value[i] = v.label()
        elif isinstance(value, BuildTarget):
            value = f'"{value.label()}"'
        elif isinstance(value, BazelVariable):
            # Print variable's name
            value = value.name

        # otherwise, use default string conversion
        text += f'{" " * indent}{key} = {value},\n'

    text += ")"
    return text


def generate_bazel_files(
    project: Project,
    output_path: Path,
):
    """Generates Bazel files for a project in specified output directory.

    Args:
        project: MCUXpresso project to output
        output_path: Path to output directory
    """
    print("# Generating bazel files... ")

    output_path.mkdir(parents=True, exist_ok=True)

    module_file = output_path / "MODULE.bazel"
    module_file.touch()

    build_file = output_path / "BUILD.bazel"
    with open(build_file, "w") as f:
        f.write(BUILDFILE_HEADER)

        for target in generate_bazel_targets(project):
            f.write(f"{target_to_bazel(target)}\n")
