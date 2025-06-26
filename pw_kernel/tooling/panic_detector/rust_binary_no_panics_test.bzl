# Copyright 2025 The Pigweed Authors
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
"""Rule to validate a rust binary contains no panics.
"""

load("//pw_build:compatibility.bzl", "incompatible_with_mcu")

def _disable_tests_transition_impl(settings, _attr):
    new_settings = dict(settings)
    new_settings["//pw_kernel:enable_tests"] = False
    return [new_settings]

disable_tests_transition = transition(
    implementation = _disable_tests_transition_impl,
    inputs = [],
    outputs = ["//pw_kernel:enable_tests"],
)

def _rust_binary_no_panics_test_impl(ctx):
    binary = ctx.files.binary[0]
    run_script = ctx.actions.declare_file("%s.sh" % ctx.label.name)

    ctx.actions.write(run_script, "#!/bin/bash\n{panic_detector} --image {image}\n".format(
        panic_detector = ctx.executable._panic_detector.short_path,
        image = binary.short_path,
    ))

    return [DefaultInfo(
        files = depset([run_script]),
        runfiles = ctx.runfiles([
            ctx.executable._panic_detector,
            binary,
        ]),
        executable = run_script,
    )]

_rust_binary_no_panics_test = rule(
    implementation = _rust_binary_no_panics_test_impl,
    test = True,
    attrs = {
        "binary": attr.label(
            doc = "rust binary to validate",
            # When checking for panics, ensure the binary to
            # be checked is compiled without the tests, as test
            # code, is, well, panicky by design...
            cfg = disable_tests_transition,
            allow_single_file = True,
            mandatory = True,
        ),
        "_panic_detector": attr.label(
            executable = True,
            cfg = "exec",
            default = "//pw_kernel/tooling/panic_detector:panic_detector",
        ),
    },
    doc = "Check whether the rust binary contains any panics.",
)

def rust_binary_no_panics_test(name, binary, **kwargs):
    """Check whether the rust binary contains any panics.

    Args:
        name: Name of the target
        binary: Target to check for panics
        **kwargs: Args to pass through to the underlying rule.
    """

    if kwargs.get("target_compatible_with") == None:
        kwargs["target_compatible_with"] = incompatible_with_mcu()

    tags = kwargs.get("tags", default = [])
    tags.append("kernel_panic_test")
    kwargs["tags"] = tags

    _rust_binary_no_panics_test(
        name = name,
        binary = binary,
        **kwargs
    )
