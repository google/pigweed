# Copyright 2020 The Pigweed Authors
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
"""Tools for compiling and importing Python protos on the fly."""

import logging
import os
from pathlib import Path
import subprocess
import shlex
import tempfile
from types import ModuleType
from typing import Dict, Iterable, List, Set, Tuple, Union
import importlib.util

_LOG = logging.getLogger(__name__)

PathOrStr = Union[Path, str]


def compile_protos(
    output_dir: PathOrStr,
    proto_files: Iterable[PathOrStr],
    includes: Iterable[PathOrStr] = ()) -> None:
    """Compiles proto files for Python by invoking the protobuf compiler.

    Proto files not covered by one of the provided include paths will have their
    directory added as an include path.
    """
    proto_paths: List[Path] = [Path(f).resolve() for f in proto_files]
    include_paths: Set[Path] = set(Path(d).resolve() for d in includes)

    for path in proto_paths:
        if not any(include in path.parents for include in include_paths):
            include_paths.add(path.parent)

    cmd: Tuple[PathOrStr, ...] = (
        'protoc',
        '--python_out',
        os.path.abspath(output_dir),
        *(f'-I{d}' for d in include_paths),
        *proto_paths,
    )

    _LOG.debug('%s', shlex.join(str(c) for c in cmd))
    process = subprocess.run(cmd, capture_output=True)

    if process.returncode:
        _LOG.error('protoc invocation failed!\n%s\n%s',
                   shlex.join(str(c) for c in cmd), process.stderr.decode())
        process.check_returncode()


def _import_module(name: str, path: str) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[union-attr]
    return module


def import_modules(directory: PathOrStr) -> Iterable[ModuleType]:
    """Imports modules in a directory and yields them."""
    parent = os.path.dirname(directory)

    for dirpath, _, files in os.walk(directory):
        path_parts = os.path.relpath(dirpath, parent).split(os.sep)

        for file in files:
            name, ext = os.path.splitext(file)

            if ext == '.py':
                yield _import_module(f'{".".join(path_parts)}.{name}',
                                     os.path.join(dirpath, file))


def compile_and_import(proto_files: Iterable[PathOrStr],
                       includes: Iterable[PathOrStr] = (),
                       output_dir: PathOrStr = None) -> Iterable[ModuleType]:
    """Compiles protos and imports their modules; yields the proto modules.

    Args:
      proto_files: paths to .proto files to compile
      includes: include paths to use for .proto compilation
      output_dir: where to place the generated modules; a temporary directory is
          used if omitted

    Yields:
      the generated protobuf Python modules
    """

    if output_dir:
        compile_protos(output_dir, proto_files, includes)
        yield from import_modules(output_dir)
    else:
        with tempfile.TemporaryDirectory(prefix='protos_') as tempdir:
            compile_protos(tempdir, proto_files, includes)
            yield from import_modules(tempdir)


def compile_and_import_file(proto_file: PathOrStr,
                            includes: Iterable[PathOrStr] = (),
                            output_dir: PathOrStr = None) -> ModuleType:
    """Compiles and imports the module for a single .proto file."""
    return next(iter(compile_and_import([proto_file], includes, output_dir)))


class _ProtoPackage:
    """Used by the Library class for accessing protocol buffer modules."""
    def __init__(self, package: str):
        self._packages: Dict[str, _ProtoPackage] = {}
        self._modules: List[ModuleType] = []
        self._package = package

    def __getattr__(self, attr: str):
        """Descends into subpackages or access proto entities in a package."""
        if attr in self._packages:
            return self._packages[attr]

        for module in self._modules:
            if hasattr(module, attr):
                return getattr(module, attr)

        raise AttributeError(
            f'Proto package "{self._package}" does not contain "{attr}"')


class Library:
    """A collection of protocol buffer modules sorted by package.

    In Python, each .proto file is compiled into a Python module. The Library
    class makes it simple to navigate a collection of Python modules
    corresponding to .proto files, without relying on the location of these
    compiled modules.

    Proto messages and other types can be directly accessed by their protocol
    buffer package name. For example, the foo.bar.Baz message can be accessed
    in a Library called `protos` as:

      protos.packages.foo.bar.Baz

    A Library also provides the modules_by_package dictionary, for looking up
    the list of modules in a particular package, and the modules() generator
    for iterating over all modules.
    """
    def __init__(self, modules: Iterable[ModuleType]):
        """Constructs a Library from an iterable of modules.

        A Library can be constructed with modules dynamically compiled by
        compile_and_import. For example:

            protos = Library(compile_and_import(list_of_proto_files))
        """
        self.modules_by_package: Dict[str, List[ModuleType]] = {}
        self.packages = _ProtoPackage('')

        for module in modules:
            package = module.DESCRIPTOR.package  # type: ignore[attr-defined]
            self.modules_by_package.setdefault(package, []).append(module)

            entry = self.packages
            subpackages = package.split('.')

            for i, subpackage in enumerate(subpackages, 1):
                if subpackage not in entry._packages:
                    entry._packages[subpackage] = _ProtoPackage('.'.join(
                        subpackages[:i]))

                entry = entry._packages[subpackage]

            entry._modules.append(module)

    def modules(self) -> Iterable[ModuleType]:
        """Allows iterating over all protobuf modules in this library."""
        for module_list in self.modules_by_package.values():
            yield from module_list
