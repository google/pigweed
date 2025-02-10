# Copyright 2021 The Pigweed Authors
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
"""Parses components for a given manifest"""

import collections
import fnmatch
import os
import pathlib
import xml.etree.ElementTree
from dataclasses import dataclass, field
from typing import Collection


def _element_is_compatible_with_device_core(
    element: xml.etree.ElementTree.Element, device_core: str | None
) -> bool:
    """Check whether element is compatible with the given core.

    Args:
        element: element to check.
        device_core: name of core to filter sources for.

    Returns:
        True if element can be used, False otherwise.
    """
    if device_core is None:
        return True

    value = element.attrib.get('device_cores', None)
    if value is None:
        return True

    device_cores = value.split()
    return device_core in device_cores


def _element_matches(
    component_id: str, patterns: Collection[str] | None
) -> bool:
    """Check whether component id matches any of provided patterns.

    Args:
        id: component id to check.
        patterns: list of patterns to compare against

    Returns:
        True if match is found, else False.
    """
    if patterns is None:
        return False

    return (
        next(filter(lambda p: fnmatch.fnmatch(component_id, p), patterns), None)
        is not None
    )


def _parse_components(
    root: xml.etree.ElementTree.Element,
    exclude: Collection[str] | None,
    device_core: str | None = None,
) -> dict[str, tuple[xml.etree.ElementTree.Element, pathlib.Path | None]]:
    """Parse <component> manifest stanza.

    Schema:
        <component id="{component_id}" package_base_path="component"
                   device_cores="{device_core}...">
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.

    Returns:
        (element, base_path) for the component, or (None, None).
    """
    components = dict()

    xpath = "./components/component"
    elements = root.findall(xpath)

    for element in elements:
        if not _element_is_compatible_with_device_core(element, device_core):
            continue

        component_id = element.attrib["id"]

        if exclude is not None and _element_matches(component_id, exclude):
            continue

        base_path_attr = element.attrib.get("package_base_path", None)
        base_path = (
            pathlib.Path(base_path_attr) if base_path_attr is not None else None
        )

        components[component_id] = (element, base_path)

    return components


def _parse_define(define: xml.etree.ElementTree.Element) -> str:
    """Parse <define> manifest stanza.

    Schema:
        <define name="EXAMPLE" value="1"/>
        <define name="OTHER"/>

    Args:
        define: XML Element for <define>.

    Returns:
        str with a value NAME=VALUE or NAME.
    """
    name = define.attrib["name"]
    value = define.attrib.get("value", None)
    if value is None:
        return name

    return f"{name}={value}"


def parse_defines(
    element: xml.etree.ElementTree.Element, device_core: str | None = None
) -> list[str]:
    """Parse pre-processor definitions for a component.

    Schema:
        <defines>
          <define name="EXAMPLE" value="1" device_cores="{device_core}..."/>
          <define name="OTHER" device_cores="{device_core}..."/>
        </defines>

    Args:
        element: XML Element of component.
        device_core: name of core to filter sources for.

    Returns:
        list of str NAME=VALUE or NAME for the component.
    """
    xpath = "./defines/define"
    return [
        _parse_define(define)
        for define in element.findall(xpath)
        if _element_is_compatible_with_device_core(define, device_core)
    ]


def _parse_include_path(
    include_path: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None = None,
) -> pathlib.Path:
    """Parse <include_path> manifest stanza.

    Schema:
        <include_path relative_path="./" type="c_include"/>

    Args:
        include_path: XML Element for <input_path>.
        base_path: prefix for paths.

    Returns:
        Path, prefixed with `base_path`.
    """
    path = pathlib.Path(include_path.attrib["relative_path"])
    # workaround for bug in mcux-sdk-middleware-edgefast-bluetooth manifest
    # in this manifest, relative path to ethermind isn't from base path,
    # but from manifest path.
    if base_path is not None and base_path == pathlib.Path(
        "../../wireless/ethermind"
    ):
        path = pathlib.Path(*path.parts[3:])
    if base_path is None:
        return path
    return base_path / path


def parse_include_paths(
    element: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None,
    manifest_path: pathlib.Path,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    """Parse include directories for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <include_paths>
            <include_path relative_path="./" type="c_include"
                          device_cores="{device_core}..."/>
          </include_paths>
        </component>

    Args:
        element: XML Element of component.
        base_path: prefix for paths.
        manifest_path: base path of manifest this component is from.
        device_core: name of core to filter sources for.

    Returns:
        list of include directories for the component.
    """
    include_paths: list[pathlib.Path] = list()
    for include_type in ("c_include", "asm_include"):
        xpath = f'./include_paths/include_path[@type="{include_type}"]'
        parent = manifest_path.parent

        include_paths.extend(
            parent / _parse_include_path(include_path, base_path)
            for include_path in element.findall(xpath)
            if _element_is_compatible_with_device_core(
                include_path, device_core
            )
        )
    return include_paths


def _parse_sources(
    source_type: str,
    component_id: str,
    element: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None,
    manifest_path: pathlib.Path,
    device_core: str | None = None,
    attrib: dict[str, str] | None = None,
) -> list[pathlib.Path]:
    """Parse source files for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="src" device_cores="{device_core}...">
            <files mask="example.cc"/>
          </source>
        </component>

    Args:
        source_type: type of source to search for.
        component_id: id of component to return.
        element: XML Element of component.
        base_path: prefix for paths.
        manifest_path: base path of manifest this component is from.
        device_core: name of core to filter sources for.
        attrib: additional attributes to filter sources with.

    Returns:
        list of source files for the component.
    """
    sources = list()

    xpath = f'./source[@type="{source_type}"]'
    for source in element.findall(xpath):
        if not _element_is_compatible_with_device_core(source, device_core):
            continue
        if attrib is not None and not attrib.items() <= source.attrib.items():
            continue

        relative_path = pathlib.Path(source.attrib["relative_path"])
        # workaround for bug in mcux-sdk-middleware-edgefast-bluetooth manifest
        # in this manifest, relative path to ethermind isn't from base path,
        # but from manifest path. In case of libraries, project relative path
        # points to not existing file, but in case of other files, it is correct
        if base_path is not None and base_path == pathlib.Path(
            "../../wireless/ethermind"
        ):
            if component_id in [
                # pylint: disable=line-too-long
                "middleware.edgefast_bluetooth.ble.ethermind.lib.cm33.MIMXRT595S",
                "middleware.edgefast_bluetooth.config.ethermind.MIMXRT595S",
                "middleware.edgefast_wifi_nxp.MIMXRT595S",
                # pylint: enable=line-too-long
            ]:
                relative_path = pathlib.Path(*relative_path.parts[3:])
            else:
                relative_path = pathlib.Path(
                    source.attrib["project_relative_path"]
                )

        if base_path is not None:
            relative_path = base_path / relative_path

        parent = manifest_path.parent
        for file in source.findall("./files"):
            filename = pathlib.Path(file.attrib["mask"])
            # Skip linker scripts, pigweed uses its own linker script.
            if filename.suffix != ".ldt":
                source_absolute = parent / relative_path / filename
                sources.append(source_absolute)
    return sources


def parse_headers(
    component_id: str,
    element: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None,
    manifest_path: pathlib.Path,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    """Parse header files for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="c_include"
                  device_cores="{device_core}...">
            <files mask="example.h"/>
          </source>
        </component>

    Args:
        component_id: id of component to return.
        element: XML Element of component.
        base_path: prefix for paths.
        manifest_path: base path of manifest this component is from.
        device_core: name of core to filter sources for.

    Returns:
        list of header files for the component.
    """
    return _parse_sources(
        "c_include",
        component_id,
        element,
        base_path,
        manifest_path,
        device_core,
    )


def parse_sources(
    component_id: str,
    element: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None,
    manifest_path: pathlib.Path,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    """Parse source files for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="src" device_cores="{device_core}...">
            <files mask="example.cc"/>
          </source>
        </component>

    Args:
        component_id: id of component to return.
        element: XML Element of component.
        base_path: prefix for paths.
        manifest_path: base path of manifest this component is from.
        device_core: name of core to filter sources for.

    Returns:
        list of source files for the component.
    """
    sources = []
    for source_type in ("src", "src_c", "src_cpp", "asm_include"):
        sources.extend(
            _parse_sources(
                source_type,
                component_id,
                element,
                base_path,
                manifest_path,
                device_core,
            )
        )
    return sources


def parse_libs(
    component_id: str,
    element: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None,
    manifest_path: pathlib.Path,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    # pylint: disable=line-too-long
    """Parse pre-compiled libraries for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source toolchain="armgcc" relative_path="./" type="lib" device_cores="{device_core}...">
            <files mask="example.a"/>
          </source>
        </component>

    Args:
        component_id: id of component to return.
        element: XML Element of component.
        base_path: prefix for paths.
        manifest_path: base path of manifest this component is from.
        device_core: name of core to filter sources for.

    Returns:
        list of pre-compiler libraries for the component.
    """
    # pylint: enable=line-too-long
    return _parse_sources(
        "lib",
        component_id,
        element,
        base_path,
        manifest_path,
        device_core,
        attrib={'toolchain': 'armgcc'},
    )


def _parse_dependency(
    dependency: xml.etree.ElementTree.Element,
    include: Collection[str],
    exclude: Collection[str] | None,
    multi: bool = False,
) -> list[str]:
    """Parse <all>, <any_of>, and <component_dependency> manifest stanzas.

    Schema:
        <all>
          <component_dependency value="component"/>
          <component_dependency value="component"/>
          <any_of>
            <component_dependency value="component"/>
            <component_dependency value="component"/>
          </any_of>
        </all>

    Args:
        dependency: XML Element of dependency.
        include: list of explicit includes
        exclude: list of explicit excludes

    Returns:
        list of component id dependencies.
    """
    if dependency.tag == "component_dependency":
        component_id = dependency.attrib["value"]

        return (
            [component_id]
            if multi or not _element_matches(component_id, exclude)
            else []
        )

    if dependency.tag == "all":
        dependencies = list()
        for subdep in dependency:
            dependencies.extend(
                _parse_dependency(subdep, include, exclude, True)
            )
        return dependencies

    if dependency.tag == "any_of":
        for subdep in dependency:
            sub = _parse_dependency(subdep, include, exclude, True)
            if all(map(lambda id: _element_matches(id, include), sub)):
                return sub

    # For other tags, simply return no dependencies
    return []


def parse_dependencies(
    element: xml.etree.ElementTree.Element,
    include: Collection[str],
    exclude: Collection[str] | None,
) -> list[str]:
    """Parse the list of dependencies for a component.

    Schema:
        <dependencies>
          <all>
            <component_dependency value="component"/>
            <component_dependency value="component"/>
            <any_of>
              <component_dependency value="component"/>
              <component_dependency value="component"/>
            </any_of>
          </all>
        </dependencies>

    Args:
        element: XML Element of component.
        include: list of explicit includes
        exclude: list of explicit excludes

    Returns:
        list of component id dependencies of the component.
    """
    dependencies = list()
    xpath = './dependencies/*'
    for dependency in element.findall(xpath):
        dependencies.extend(_parse_dependency(dependency, include, exclude))
    return dependencies


@dataclass
class Component:
    # pylint: disable=line-too-long
    """Individual MCUXpresso component.

    Properties:
        id: id of this component.
        defines: list of compiler definitions to build this component.
        include_dirs: list of include directory paths needed for this component.
        headers: list of header paths exported by this component.
        sources: list of source file paths built as part of this component.
        libs: list of paths to prebuilt libraries required by this component.
        dependencies: set of component_ids required to build this component.
        alwayslink: whether this component contains symbols that should always be included.
        private: whether this component should be hidden from public api.
    """

    # pylint: enable=line-too-long

    id: str
    defines: list[str] = field(default_factory=list)
    include_dirs: list[pathlib.Path] = field(default_factory=list)
    headers: list[pathlib.Path] = field(default_factory=list)
    sources: list[pathlib.Path] = field(default_factory=list)
    libs: list[pathlib.Path] = field(default_factory=list)
    dependencies: set[str] = field(default_factory=set)
    alwayslink: bool = False
    private: bool = False

    @classmethod
    def from_manifest(
        cls,
        component_id: str,
        element: xml.etree.ElementTree.Element,
        include: Collection[str],
        exclude: Collection[str] | None,
        base_path: pathlib.Path | None,
        manifest_path: pathlib.Path,
        root_path: pathlib.Path,
        device_core: str | None = None,
    ):
        """Create component definition.

        Args:
            component_id: id of this component.
            element: XML element of this particular component.
            include: list of explicit includes.
            exclude: optional list of explicit excludes.
            base_path: base path of this component.
            manifest_path: path to manifest this component is from.
            root_path: path to root of project directory.
            device_core: name of core to filter sources for.
        """

        # This component contains flash configuration that is required
        # by the bootrom. Since the symbols contained aren't directly
        # used by applications, we have to mark it as `alwayslink`,
        # otherwise they will be discarded by the linker when producing
        # the final binary
        alwayslink = (
            component_id
            == "platform.drivers.flash_config.evkmimxrt595.MIMXRT595S"
        )

        def relative_to_root(paths: list[pathlib.Path]) -> list[pathlib.Path]:
            try:
                return [path.resolve().relative_to(root_path) for path in paths]
            except ValueError:
                return []

        defines = parse_defines(element, device_core)
        include_paths = relative_to_root(
            parse_include_paths(element, base_path, manifest_path, device_core)
        )
        headers = relative_to_root(
            parse_headers(
                component_id, element, base_path, manifest_path, device_core
            )
        )
        sources = relative_to_root(
            parse_sources(
                component_id, element, base_path, manifest_path, device_core
            )
        )
        libs = relative_to_root(
            parse_libs(
                component_id, element, base_path, manifest_path, device_core
            )
        )
        deps = set(parse_dependencies(element, include, exclude))

        return cls(
            component_id,
            defines,
            include_paths,
            headers,
            sources,
            libs,
            deps,
            alwayslink,
        )

    def merged(self, other: "Component") -> "Component":
        """Creates new component with properties from self and other"""
        merge_list = lambda a, b: sorted(set(a).union(b))

        new_self = Component(
            self.id,
            merge_list(self.defines, other.defines),
            merge_list(self.include_dirs, other.include_dirs),
            merge_list(self.headers, other.headers),
            merge_list(self.sources, other.sources),
            merge_list(self.libs, other.libs),
            self.dependencies.union(other.dependencies),
        )

        new_self.dependencies.difference_update((self.id, other.id))
        return new_self


def create_components(
    manifests: list[tuple[xml.etree.ElementTree.Element, pathlib.Path]],
    root_path: pathlib.Path,
    include: Collection[str],
    exclude: Collection[str] | None = None,
    device_core: str | None = None,
) -> dict[str, Component]:
    """Create component definitions from a list of component ids.

    Args:
        manifests: list of root manifests.
        root_path: path to root of project directory.
        include: collection of component ids included in the project.
        exclude: container of component ids excluded from the project.
        device_core: name of core to filter sources for.

    Returns:
        dictionary of component_ids to components.
    """

    components: dict[str, Component] = dict()
    for root, manifest_path in manifests:
        parsed_components = _parse_components(root, exclude, device_core)

        components.update(
            (
                component_id,
                Component.from_manifest(
                    component_id,
                    element,
                    include,
                    exclude,
                    base_path,
                    manifest_path,
                    root_path,
                    device_core,
                ),
            )
            for (
                component_id,
                (element, base_path),
            ) in parsed_components.items()
        )

    required = set()
    excluded = set()
    deps: collections.deque[str] = collections.deque()

    for component_id, component in components.items():
        # Check if element was selected in `include` parameters
        if _element_matches(component_id, include):
            required.add(component_id)
            deps.extend(component.dependencies)

    while len(deps) > 0:
        dep_id = deps.popleft()
        if dep_id in required or dep_id in excluded:
            continue

        # Check if element was explicitly disabled in `exclude` parameters
        if _element_matches(dep_id, exclude):
            excluded.add(dep_id)
            continue

        required.add(dep_id)
        deps.extend(components[dep_id].dependencies)

    for component in components.values():
        component.dependencies.difference_update(excluded)

    return {
        component_id: component
        for (component_id, component) in components.items()
        if component_id in required
    }


class Project:
    """Self-contained MCUXpresso project.

    Properties:
        component_ids: list of component ids compromising the project.
        defines: list of compiler definitions to build the project.
        include_dirs: list of include directory paths needed for the project.
        headers: list of header paths exported by the project.
        sources: list of source file paths built as part of the project.
        libs: list of libraries linked to the project.
        root: path to project's root directory.
    """

    @classmethod
    def from_file(
        cls,
        manifest_path: pathlib.Path,
        root_path: pathlib.Path,
        include: Collection[str] | None = None,
        exclude: Collection[str] | None = None,
        device_core: str | None = None,
    ):
        """Create a self-contained project with the specified components.

        Args:
            manifest_path: path to SDK manifest XML.
            root_path: path to project's root directory.
            include: collection of component ids included in the project.
            exclude: container of component ids excluded from the project.
            device_core: name of core to filter sources for.
        """
        tree = xml.etree.ElementTree.parse(manifest_path)

        # paths in the manifest are relative paths from manifest folder
        roots = [(tree.getroot(), manifest_path)]

        xpath = "./manifest_includes/*"
        for manifest_include in roots[0][0].findall(xpath):
            include_manifest_path = manifest_path.parents[0] / pathlib.Path(
                manifest_include.attrib["path"]
            )
            roots.append(
                (
                    xml.etree.ElementTree.parse(
                        include_manifest_path
                    ).getroot(),
                    include_manifest_path,
                )
            )

        if include is None:
            include = ["*"]

        result = cls(
            roots,
            root_path,
            include=include,
            exclude=exclude,
            device_core=device_core,
        )

        return result

    def __init__(
        self,
        manifest: list[tuple[xml.etree.ElementTree.Element, pathlib.Path]],
        root_path: pathlib.Path,
        include: Collection[str],
        exclude: Collection[str] | None = None,
        device_core: str | None = None,
    ):
        """Create a self-contained project with the specified components.

        Args:
            manifest: list of parsed manifest XMLs.
            root_path: path to project's root directory.
            include: collection of component ids included in the project.
            exclude: container of component ids excluded from the project.
            device_core: name of core to filter sources for.
        """
        self.root = root_path.resolve()

        self.components = create_components(
            manifest, self.root, include, exclude, device_core
        )

    def cleanup_unknown_files(self):
        """
        Removes all files that aren't part of the project from root directory
        """

        # Safety check - don't remove anything if project root is unset
        if not self.root:
            return

        # We're using abspath from os module since we want normalized path
        # without resolving symlinks
        abspath = lambda path: pathlib.Path(os.path.abspath(self.root / path))

        src_files: set[pathlib.Path] = set()
        for component in self.components.values():
            src_files.update(
                map(abspath, component.sources),
                map(abspath, component.headers),
                map(abspath, component.libs),
            )

        for root, dirs, files in os.walk(self.root):
            for file in map(pathlib.Path, files):
                target = abspath(root / file)
                if not target.is_symlink() and target.is_dir():
                    continue
                if target not in src_files:
                    os.remove(target)
            for dirpath in map(pathlib.Path, dirs):
                try:
                    (root / dirpath).rmdir()
                # Skip non-empty dirs
                except OSError:
                    continue
