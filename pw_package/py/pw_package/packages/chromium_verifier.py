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
"""Install and check status of BoringSSL + Chromium verifier."""

import os
import pathlib
from typing import Sequence
import pw_package.git_repo
import pw_package.package_manager

# List of sources to checkout for chromium verifier.
# The list is hand-picked. It is currently only tested locally (i.e. the list
# compiles and can run certificate chain verification). Unittest will be added
# in pw_tls_client that uses the this package, so that it can be used as a
# criterion for rolling.
CHROMIUM_VERIFIER_LIBRARY_SOURCES = [
    'base/as_const.h',
    'base/atomic_ref_count.h',
    'base/atomicops.h',
    'base/atomicops_internals_portable.h',
    'base/base_export.h',
    'base/callback_forward.h',
    'base/compiler_specific.h',
    'base/component_export.h',
    'base/containers/checked_iterators.h',
    'base/containers/checked_range.h',
    'base/containers/contains.h',
    'base/containers/contiguous_iterator.h',
    'base/containers/flat_map.h',
    'base/containers/flat_tree.h',
    'base/containers/span.h',
    'base/containers/stack_container.h',
    'base/containers/util.h',
    'base/cxx17_backports.h',
    'base/dcheck_is_on.h',
    'base/debug/alias.h',
    'base/export_template.h',
    'base/functional/identity.h',
    'base/functional/invoke.h',
    'base/functional/not_fn.h',
    'base/gtest_prod_util.h',
    'base/location.cc',
    'base/location.h',
    'base/macros.h',
    'base/memory/ptr_util.h',
    'base/memory/ref_counted.cc',
    'base/memory/ref_counted.h',
    'base/memory/scoped_refptr.h',
    'base/metrics/bucket_ranges.h',
    'base/metrics/histogram.h',
    'base/metrics/histogram_base.h',
    'base/metrics/histogram_functions.h',
    'base/metrics/histogram_samples.h',
    'base/no_destructor.h',
    'base/notreached.h',
    'base/numerics/checked_math.h',
    'base/numerics/checked_math_impl.h',
    'base/numerics/clamped_math.h',
    'base/numerics/clamped_math_impl.h',
    'base/numerics/safe_conversions.h',
    'base/numerics/safe_conversions_arm_impl.h',
    'base/numerics/safe_conversions_impl.h',
    'base/numerics/safe_math.h',
    'base/numerics/safe_math_arm_impl.h',
    'base/numerics/safe_math_clang_gcc_impl.h',
    'base/numerics/safe_math_shared_impl.h',
    'base/optional.h',
    'base/ranges/algorithm.h',
    'base/ranges/functional.h',
    'base/ranges/ranges.h',
    'base/scoped_clear_last_error.h',
    'base/scoped_generic.h',
    'base/sequence_checker.h',
    'base/sequence_checker_impl.h',
    'base/stl_util.h',
    'base/strings/char_traits.h',
    'base/strings/strcat.h',
    'base/strings/string_number_conversions.cc',
    'base/strings/string_number_conversions.h',
    'base/strings/string_number_conversions_internal.h',
    'base/strings/string_piece.h',
    'base/strings/string_piece_forward.h',
    'base/strings/string_split.cc',
    'base/strings/string_split.h',
    'base/strings/string_split_internal.h',
    'base/strings/string_util.cc',
    'base/strings/string_util.h',
    'base/strings/string_util_internal.h',
    'base/strings/string_util_posix.h',
    'base/strings/stringprintf.cc',
    'base/strings/stringprintf.h',
    'base/strings/utf_string_conversion_utils.cc',
    'base/strings/utf_string_conversion_utils.h',
    'base/strings/utf_string_conversions.cc',
    'base/strings/utf_string_conversions.h',
    'base/supports_user_data.cc',
    'base/supports_user_data.h',
    'base/sys_byteorder.h',
    'base/template_util.h',
    'base/third_party/double_conversion/double-conversion/double-conversion.h',
    'base/third_party/double_conversion/double-conversion/double-to-string.h',
    'base/third_party/double_conversion/double-conversion/string-to-double.h',
    'base/third_party/double_conversion/double-conversion/utils.h',
    'base/third_party/icu/icu_utf.h',
    'base/third_party/nspr/prtime.h',
    'base/thread_annotations.h',
    'base/threading/platform_thread.h',
    'base/threading/thread_collision_warner.h',
    'base/time/time.cc',
    'base/time/time.h',
    'base/time/time_override.h',
    'base/trace_event/base_tracing_forward.h',
    'base/value_iterators.h',
    'base/values.h',
    'build/buildflag.h',
    'crypto/crypto_export.h',
    'crypto/openssl_util.cc',
    'crypto/openssl_util.h',
    'crypto/sha2.h',
    'net/base/ip_address.cc',
    'net/base/ip_address.h',
    'net/base/net_export.h',
    'net/base/parse_number.h',
    'net/cert/internal/cert_error_id.cc',
    'net/cert/internal/cert_error_id.h',
    'net/cert/internal/cert_error_params.cc',
    'net/cert/internal/cert_error_params.h',
    'net/cert/internal/cert_errors.cc',
    'net/cert/internal/cert_errors.h',
    'net/cert/internal/cert_issuer_source.h',
    'net/cert/internal/cert_issuer_source_static.cc',
    'net/cert/internal/cert_issuer_source_static.h',
    'net/cert/internal/certificate_policies.cc',
    'net/cert/internal/certificate_policies.h',
    'net/cert/internal/common_cert_errors.cc',
    'net/cert/internal/common_cert_errors.h',
    'net/cert/internal/extended_key_usage.cc',
    'net/cert/internal/extended_key_usage.h',
    'net/cert/internal/general_names.cc',
    'net/cert/internal/general_names.h',
    'net/cert/internal/name_constraints.cc',
    'net/cert/internal/name_constraints.h',
    'net/cert/internal/parse_certificate.cc',
    'net/cert/internal/parse_certificate.h',
    'net/cert/internal/parse_name.cc',
    'net/cert/internal/parse_name.h',
    'net/cert/internal/parsed_certificate.cc',
    'net/cert/internal/parsed_certificate.h',
    'net/cert/internal/path_builder.cc',
    'net/cert/internal/path_builder.h',
    'net/cert/internal/signature_algorithm.cc',
    'net/cert/internal/signature_algorithm.h',
    'net/cert/internal/simple_path_builder_delegate.cc',
    'net/cert/internal/simple_path_builder_delegate.h',
    'net/cert/internal/trust_store.cc',
    'net/cert/internal/trust_store.h',
    'net/cert/internal/trust_store_in_memory.cc',
    'net/cert/internal/trust_store_in_memory.h',
    'net/cert/internal/verify_certificate_chain.cc',
    'net/cert/internal/verify_certificate_chain.h',
    'net/cert/internal/verify_name_match.cc',
    'net/cert/internal/verify_name_match.h',
    'net/cert/internal/verify_signed_data.cc',
    'net/cert/internal/verify_signed_data.h',
    'net/der/encode_values.cc',
    'net/der/encode_values.h',
    'net/der/input.cc',
    'net/der/input.h',
    'net/der/parse_values.cc',
    'net/der/parse_values.h',
    'net/der/parser.cc',
    'net/der/parser.h',
    'net/der/tag.cc',
    'net/der/tag.h',
    'testing/gtest/include/gtest/gtest_prod.h',
    'third_party/abseil-cpp/absl/base/attributes.h',
    'third_party/abseil-cpp/absl/base/config.h',
    'third_party/abseil-cpp/absl/base/internal/identity.h',
    'third_party/abseil-cpp/absl/base/internal/inline_variable.h',
    'third_party/abseil-cpp/absl/base/internal/invoke.h',
    'third_party/abseil-cpp/absl/base/internal/throw_delegate.h',
    'third_party/abseil-cpp/absl/base/macros.h',
    'third_party/abseil-cpp/absl/base/optimization.h',
    'third_party/abseil-cpp/absl/base/options.h',
    'third_party/abseil-cpp/absl/base/policy_checks.h',
    'third_party/abseil-cpp/absl/base/port.h',
    'third_party/abseil-cpp/absl/meta/type_traits.h',
    'third_party/abseil-cpp/absl/strings/string_view.h',
    'third_party/abseil-cpp/absl/time/civil_time.h',
    'third_party/abseil-cpp/absl/time/clock.h',
    'third_party/abseil-cpp/absl/time/internal/cctz/include/cctz/civil_time.h',
    'third_party/abseil-cpp/absl/types/optional.h',
    'third_party/abseil-cpp/absl/time/internal/cctz/include/cctz/time_zone.h',
    'third_party/abseil-cpp/absl/time/time.h',
    'third_party/abseil-cpp/absl/types/variant.h',
    'third_party/abseil-cpp/absl/utility/utility.h',
    'third_party/abseil-cpp/absl/utility/utility.h',
    'third_party/boringssl',
    'time/internal/cctz/include/cctz/civil_time_detail.h',
    'url/gurl.h',
    'url/third_party/mozilla/url_parse.h',
    'url/url_canon.h',
    'url/url_canon_ip.h',
    'url/url_canon_stdstring.h',
    'url/url_constants.h',
]

CHROMIUM_VERIFIER_UNITTEST_SOURCES = [
    # TODO(pwbug/394): Look into in necessary unittests to port.
    'net/cert/internal/path_builder_unittest.cc'
]

CHROMIUM_VERIFIER_SOURCES = CHROMIUM_VERIFIER_LIBRARY_SOURCES +\
    CHROMIUM_VERIFIER_UNITTEST_SOURCES

HEADER = """
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
#
# The file is auto-generated when chromium verifier is installed from pw_package.
# See //pw_package/py/pw_package/packages/boringssl.py for more detail.

"""


def generate_chromium_verifier_source_list_gni(path: pathlib.Path):
    """Generate a .gni file containing the list of sources to compile"""
    with open(path / 'sources.gni', 'w') as gni:
        gni.write(HEADER)
        gni.write('chromium_verifier_sources = [\n')
        for source in [
                src for src in CHROMIUM_VERIFIER_LIBRARY_SOURCES
                if src.endswith('.cc')
        ]:
            gni.write(f'  "{source}",\n')
        gni.write(']\n\n')

        gni.write('chromium_verifier_unittest_sources = [\n')
        for source in [
                src for src in CHROMIUM_VERIFIER_UNITTEST_SOURCES
                if src.endswith('.cc')
        ]:
            gni.write(f'  "{source}",\n')
        gni.write(']\n')


def chromium_verifier_repo_path(
        chromium_verifier_install: pathlib.Path) -> pathlib.Path:
    """Return the sub-path for repo checkout of chromium verifier"""
    return chromium_verifier_install / 'src'


def chromium_third_party_boringssl_repo_path(
        chromium_verifier_repo: pathlib.Path) -> pathlib.Path:
    """Returns the path of third_party/boringssl library in chromium repo"""
    return chromium_verifier_repo / 'third_party' / 'boringssl' / 'src'


def chromium_third_party_googletest_repo_path(
        chromium_verifier_repo: pathlib.Path) -> pathlib.Path:
    """Returns the path of third_party/googletest in chromium repo"""
    return chromium_verifier_repo / 'third_party' / 'googletest' / 'src'


class ChromiumVerifier(pw_package.package_manager.Package):
    """Install and check status of Chromium Verifier"""
    def __init__(self, *args, **kwargs):
        super().__init__(*args, name='chromium_verifier', **kwargs)
        self._chromium_verifier = pw_package.git_repo.GitRepo(
            name='chromium_verifier',
            url='https://chromium.googlesource.com/chromium/src',
            commit='04ebce24d98339954fb1d2a67e68da7ca81ca47c',
            sparse_list=CHROMIUM_VERIFIER_SOURCES,
        )

        # The following is for checking out necessary headers of
        # boringssl and googletest third party libraries that chromium verifier
        # depends on. The actual complete libraries will be separate packages.

        self._boringssl = pw_package.git_repo.GitRepo(
            name='boringssl',
            url=''.join([
                'https://pigweed.googlesource.com',
                '/third_party/boringssl/boringssl'
            ]),
            commit='9f55d972854d0b34dae39c7cd3679d6ada3dfd5b',
            sparse_list=['include'],
        )

        self._googletest = pw_package.git_repo.GitRepo(
            name='googletest',
            url=''.join([
                'https://chromium.googlesource.com/',
                'external/github.com/google/googletest.git',
            ]),
            commit='53495a2a7d6ba7e0691a7f3602e9a5324bba6e45',
            sparse_list=[
                'googletest/include',
                'googlemock/include',
            ])

    def install(self, path: pathlib.Path) -> None:
        # Checkout chromium verifier
        chromium_repo = chromium_verifier_repo_path(path)
        self._chromium_verifier.install(chromium_repo)

        # Checkout third party boringssl headers
        boringssl_repo = chromium_third_party_boringssl_repo_path(
            chromium_repo)
        self._boringssl.install(boringssl_repo)

        # Checkout third party googletest headers
        googletest_repo = chromium_third_party_googletest_repo_path(
            chromium_repo)
        self._googletest.install(googletest_repo)

        # Generate a.gni file containing sources to compile.
        generate_chromium_verifier_source_list_gni(path)

    def status(self, path: pathlib.Path) -> bool:
        chromium_repo = chromium_verifier_repo_path(path)
        if not self._chromium_verifier.status(chromium_repo):
            return False

        boringssl_repo = chromium_third_party_boringssl_repo_path(
            chromium_repo)
        if not self._boringssl.status(boringssl_repo):
            return False

        googletest_repo = chromium_third_party_googletest_repo_path(
            chromium_repo)
        if not self._googletest.status(googletest_repo):
            return False

        # A source list gni file has been generated.
        if not os.path.exists(path / 'sources.gni'):
            return False

        return True

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (
            f'{self.name} installed in: {path}',
            'Enable by running "gn args out" and adding this line:',
            f'  dir_pw_third_party_chromium_verifier = {path}',
        )


pw_package.package_manager.register(ChromiumVerifier)
