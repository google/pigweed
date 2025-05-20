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

rust_binary_no_panics_test = rule(
    implementation = _rust_binary_no_panics_test_impl,
    test = True,
    attrs = {
        "binary": attr.label(
            doc = "rust binary to validate",
            cfg = "target",
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
