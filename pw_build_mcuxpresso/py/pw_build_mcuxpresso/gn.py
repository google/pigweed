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
"""GN output support."""

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

try:
    from pw_build_mcuxpresso.common import (
        BuildTarget,
        find_component_dep_cycles,
        normalize_path_list,
    )
    from pw_build_mcuxpresso.components import Component, Project
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from common import (  # type: ignore
        BuildTarget,
        find_component_dep_cycles,
        normalize_path_list,
    )
    from components import Component, Project  # type: ignore


@dataclass
class GnArg:
    """Represents single GN arg"""

    name: str
    default_value: str = ""
    doc: str | None = None

    def __str__(self) -> str:
        return self.name


@dataclass
class CondAttr:
    """Represents conditional attribute

    Properties:
        cond: attribute condition
        value: attribute value
        append: append the value instead of overwriting
    """

    cond: str
    value: Any
    append: bool = True


ARGS_FILE = "mcuxpresso.gni"

# pylint: disable=line-too-long
USER_CONFIG_ARG = GnArg(
    "pw_third_party_mcuxpresso_CONFIG",
    doc="The pw_source_set which provides the SDK specific configuration options.",
)
# pylint: enable=line-too-long

USER_CONFIG_COND = CondAttr(f'{USER_CONFIG_ARG} != ""', [USER_CONFIG_ARG])

# pylint: disable=line-too-long
BUILDFILE_HEADER = r"""### This file was auto generated. Do not edit manually. ###
import("//build_overrides/pigweed.gni")
import("$dir_pw_build/target_types.gni")
import("{}")
""".format(
    ARGS_FILE
)
# pylint: enable=line-too-long


def generate_gn_targets(project: Project) -> list[BuildTarget]:
    """Generates GN targets for a project

    Args:
        project: MCUXpresso project to output
    """
    cycles: dict[str, list[str]] = dict()
    for a, b in find_component_dep_cycles(project):
        cycles.setdefault(a, []).append(b)

    components = project.components
    targets: dict[str, BuildTarget] = dict()

    def component_target(component: Component) -> BuildTarget:
        if component.id in targets.keys():
            return targets[component.id]

        attrs: dict[str, Any] = dict()

        if component.id in cycles.keys():
            for dep_id in cycles[component.id]:
                dep = components[dep_id]

                component = component.merged(dep)
                components[component.id] = component

        public_deps: Sequence[BuildTarget | GnArg | CondAttr] = sorted(
            component_target(components[dep_id])
            for dep_id in component.dependencies
        ) + [USER_CONFIG_COND]
        sources = sorted(
            normalize_path_list(component.sources),
        )
        public = sorted(normalize_path_list(component.headers))
        include_dirs = normalize_path_list(component.include_dirs)
        libs = normalize_path_list(component.libs)

        if len(component.defines) > 0 or len(include_dirs) > 0:
            config_target = BuildTarget(
                "config",
                f"{component.id}__config",
                {
                    "defines": component.defines,
                    "include_dirs": include_dirs,
                },
            )

            targets[config_target.name] = config_target
            attrs.update({"public_configs": [config_target]})

        attrs.update(
            {
                "sources": sources,
                "public": public,
                "libs": libs,
                "public_deps": public_deps,
                "check_includes": False,
            }
        )

        target = BuildTarget(
            "pw_static_library"
            if not component.alwayslink
            else "pw_source_set",
            component.id,
            attrs,
        )
        targets[target.name] = target

        return target

    for component in project.components.values():
        component_target(component)

    return sorted(targets.values())


def declare_args(args: list[GnArg]) -> str:
    """Generates `declare_args` block"""

    text = "declare_args() {\n"

    for arg in args:
        if arg.doc:
            for line in arg.doc.splitlines():
                text += f"  # {line}\n"

        text += f'  {arg.name} = "{arg.default_value}"\n'

    text += "}"
    return text


def target_to_gn(target: BuildTarget, indent: int = 2) -> str:
    """Converts BuildTarget to GN string representation

    Args:
        target: build target
        indent: specifies amount of indentation added for each nesting level
    """
    depth = 0
    space = lambda depth: "\n" + " " * indent * depth

    cond_attrs: dict[str, dict[str, CondAttr]] = dict()

    def format_value(key: str, value: Any, depth: int) -> str | None:
        # Handle special formatting
        if isinstance(value, str):
            return '"{}"'.format(value.replace('"', r"\""))

        if isinstance(value, bool):
            return "{}".format("true" if value else "false")

        if isinstance(value, list):
            if len(value) == 0:
                return None
            if len(value) == 1:
                val = format_value(key, value[0], depth)
                if val is None:
                    return None

                return f"[ {val} ]"

            out = "["
            for v in value:
                depth += 1
                vstr = format_value(key, v, depth)

                if vstr is None:
                    depth -= 1
                    continue

                out += "{}{},".format(space(depth), vstr)
                depth -= 1

            return "{}{}]".format(out, space(depth))

        if isinstance(value, BuildTarget):
            return '"{}"'.format(value.label())

        if isinstance(value, GnArg):
            return "{}".format(value.name)

        if isinstance(value, CondAttr):
            cond_attrs.setdefault(value.cond, dict())[key] = value
            return None

        # Default: use python's default formatting
        return "{}".format(value)

    text = '{}("{}") {{'.format(target.target_type, target.name)

    empty: set[str] = set()

    depth += 1
    for key, value in sorted(target.attrs.items(), key=lambda kv: kv[0]):
        fmt_value = format_value(key, value, depth)
        if fmt_value is None:
            empty.add(key)
            continue
        text += "{}{} = {}".format(space(depth), key, fmt_value)

    for condition, attrs in cond_attrs.items():
        text += "{}if ({}) {{".format(space(depth), condition)

        depth += 1
        for key, attr in attrs.items():
            fmt_value = format_value(key, attr.value, depth)

            action = (
                "+="
                if attr.append
                and target.attrs.get(key, None)
                and key not in empty
                else "="
            )
            text += "{}{} {} {}".format(space(depth), key, action, fmt_value)

        depth -= 1
        text += "{}}}".format(space(depth))

    depth -= 1
    text += "{}}}".format(space(depth))

    return text


def generate_gn_files(
    project: Project,
    output_path: Path,
):
    """Generates GN files for a project in specified output directory.

    Args:
        project: MCUXpresso project to output
        output_path: Path to output directory
    """
    print("# Generating GN files...")

    output_path.mkdir(parents=True, exist_ok=True)

    args_file = output_path / ARGS_FILE
    with open(args_file, "w") as f:
        f.write(declare_args([USER_CONFIG_ARG]))

    build_file = output_path / "BUILD.gn"
    with open(build_file, "w") as f:
        f.write(BUILDFILE_HEADER)

        for target in generate_gn_targets(project):
            f.write(f"{target_to_gn(target)}\n")
