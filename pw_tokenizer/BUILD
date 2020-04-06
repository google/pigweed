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

load(
    "//pw_build:pigweed.bzl",
    "pw_cc_binary",
    "pw_cc_library",
    "pw_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_library(
    name = "pw_tokenizer",
    srcs = [
        "public/pw_tokenizer/config.h",
        "public/pw_tokenizer/internal/argument_types.h",
        "public/pw_tokenizer/internal/argument_types_macro_4_byte.h",
        "public/pw_tokenizer/internal/argument_types_macro_8_byte.h",
        "public/pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_128_hash_macro.h",
        "public/pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_80_hash_macro.h",
        "public/pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_96_hash_macro.h",
        "public/pw_tokenizer/internal/tokenize_string.h",
        "tokenize.cc",
    ],
    hdrs = [
        "public/pw_tokenizer/pw_tokenizer_65599_fixed_length_hash.h",
        "public/pw_tokenizer/tokenize.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_polyfill",
        "//pw_preprocessor",
        "//pw_span",
        "//pw_varint",
    ],
)

pw_cc_library(
    name = "base64",
    srcs = [
        "base64.cc",
    ],
    hdrs = [
        "public/pw_tokenizer/base64.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_base64",
        "//pw_preprocessor",
        "//pw_span",
    ],
)

pw_cc_library(
    name = "decoder",
    srcs = [
        "decode.cc",
        "detokenize.cc",
        "token_database.cc",
    ],
    hdrs = [
        "public/pw_tokenizer/detokenize.h",
        "public/pw_tokenizer/internal/decode.h",
        "public/pw_tokenizer/token_database.h",
    ],
    includes = ["public"],
    deps = [
        "//pw_span",
        "//pw_varint",
    ],
)

# Executable for generating test data for the C++ and Python detokenizers. This
# target should only be built for the host.
pw_cc_binary(
    name = "generate_decoding_test_data",
    srcs = [
        "generate_decoding_test_data.cc",
    ],
    deps = [
        ":decoder",
        ":pw_tokenizer",
        "//pw_preprocessor",
        "//pw_span",
        "//pw_varint",
    ],
)

# Executable for generating a test ELF file for elf_reader_test.py. A host
# version of this binary is checked in for use in elf_reader_test.py.
cc_binary(
    name = "elf_reader_test_binary",
    srcs = [
        "py/elf_reader_test_binary.c",
    ],
    linkopts = ["-Wl,--unresolved-symbols=ignore-all"],  # main is not defined
    deps = [
        ":pw_tokenizer",
        "//pw_varint",
    ],
)

pw_cc_test(
    name = "argument_types_test",
    srcs = [
        "argument_types_test.c",
        "argument_types_test.cc",
        "pw_tokenizer_private/argument_types_test.h",
    ],
    deps = [
        ":pw_tokenizer",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "base64_test",
    srcs = [
        "base64_test.cc",
    ],
    deps = [
        ":base64",
        "//pw_span",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "decode_test",
    srcs = [
        "decode_test.cc",
        "pw_tokenizer_private/tokenized_string_decoding_test_data.h",
        "pw_tokenizer_private/varint_decoding_test_data.h",
    ],
    deps = [
        ":decoder",
        "//pw_unit_test",
        "//pw_varint",
    ],
)

pw_cc_test(
    name = "detokenize_test",
    srcs = [
        "detokenize_test.cc",
    ],
    deps = [
        ":decoder",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "hash_test",
    srcs = [
        "hash_test.cc",
        "pw_tokenizer_private/generated_hash_test_cases.h",
    ],
    deps = [
        ":pw_tokenizer",
        "//pw_preprocessor",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "simple_tokenize_test",
    srcs = [
        "simple_tokenize_test.cc",
    ],
    deps = [
        ":pw_tokenizer",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "token_database_test",
    srcs = [
        "token_database_test.cc",
    ],
    deps = [
        ":decoder",
        "//pw_unit_test",
    ],
)

pw_cc_test(
    name = "tokenize_test",
    srcs = [
        "pw_tokenizer_private/tokenize_test.h",
        "tokenize_test.c",
        "tokenize_test.cc",
    ],
    deps = [
        ":pw_tokenizer",
        "//pw_preprocessor",
        "//pw_unit_test",
        "//pw_varint",
    ],
)

# Create a shared library for the tokenizer JNI wrapper. The include paths for
# the JNI headers must be available in the system or provided with the
# pw_java_native_interface_include_dirs variable.
filegroup(
    name = "detokenizer_jni",
    srcs = [
        "java/dev/pigweed/tokenizer/detokenizer.cc",
    ],
)
