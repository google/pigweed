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
"""Helpers for expressing compiler-specific build file logic."""

def if_compiler_is_clang(then, *, otherwise):
    return select({
        "//pw_toolchain/cc/current_toolchain:compiler_is_clang": then,
        "@rules_cc//cc/compiler:clang": then,
        "//conditions:default": otherwise,
    })

def if_compiler_is_gcc(then, *, otherwise):
    return select({
        "//pw_toolchain/cc/current_toolchain:compiler_is_gcc": then,
        "@rules_cc//cc/compiler:gcc": then,
        "//conditions:default": otherwise,
    })

def if_linker_is_clang(then, *, otherwise):
    return select({
        "//pw_toolchain/cc/current_toolchain:linker_is_clang": then,
        "@rules_cc//cc/compiler:clang": then,
        "//conditions:default": otherwise,
    })

def if_linker_is_gcc(then, *, otherwise):
    return select({
        "//pw_toolchain/cc/current_toolchain:linker_is_gcc": then,
        "@rules_cc//cc/compiler:gcc": then,
        "//conditions:default": otherwise,
    })
