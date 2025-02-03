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
"""Bazel rules for declaring Pigweed facade interface layers."""

load("@rules_cc//cc:cc_library.bzl", "cc_library")

def pw_facade(name, srcs = None, backend = None, **kwargs):
    """Create a cc_library with a facade.

    This macro simplifies instantiating Pigweed's facade pattern. It generates
    two targets:

    * cc_library with the label "name". This is the complete library target.
      Users of the functionality provided by this library should depend on this
      target.  It has a public dependency on the "backend".
    * cc_library with the label "name.facade". This library exposes only the
      headers. Implementations of the backend should depend on it.

    Args:
      name: The name of the cc_library.
      srcs: The source files of the cc_library.
      backend: The backend for the facade. This should be a label_flag or other
        target that allows swapping out the backend implementation at build
        time. (In a downstream project an alias with an "actual = select(...)"
        attribute may also be appropriate, but in upstream Pigweed use only a
        label_flag.).
      **kwargs: Passed on to cc_library.
    """
    if type(backend) != "string":
        fail(
            "The 'backend' attribute must be a single label, " +
            "got {} of type {}".format(backend, type(backend)),
        )

    facade_kwargs = dict(**kwargs)

    # A facade has no srcs, so it can only have public deps. Don't specify any
    # implementation_deps on the facade target.
    facade_kwargs.pop("implementation_deps", [])
    cc_library(
        name = name + ".facade",
        # The .facade target is not self-contained (it's missing a dependency
        # on the backend headers), so it can't be successfully clang-tidied.
        # "no-clang-tidy-headers" is a special tag defined in bazel_clang_tidy
        # that exempts a build target's headers from tidying; see
        # https://github.com/erenon/bazel_clang_tidy/pull/76.
        tags = facade_kwargs.pop("tags", []) + ["no-clang-tidy-headers"],
        **facade_kwargs
    )

    kwargs["deps"] = kwargs.get("deps", []) + [backend]
    cc_library(
        name = name,
        srcs = srcs,
        **kwargs
    )
