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
"""Internal implementation of size report tables."""

PwBloatInfo = provider(
    "Information about a pw_bloat size report",
    fields = {
        "fragment": "Path to compiled RST fragment",
    },
)

def _pw_bloat_report_impl(ctx):
    rst_output = ctx.actions.declare_file(ctx.label.name)
    script_input = ctx.actions.declare_file(ctx.label.name + "_binaries.json")

    json_metadata = {
        "binaries": [
            {
                "base": ctx.executable.base.path,
                "bloaty_config": ctx.file.bloaty_config.path,
                "label": ctx.attr.label,
                "target": ctx.executable.target.path,
            },
        ],
        "out_dir": rst_output.dirname,
        "target_name": ctx.label.name,
    }

    ctx.actions.write(script_input, json.encode(json_metadata))

    args = ctx.actions.args()
    args.add("--target-json={}".format(script_input.path))
    args.add("--generate-rst-fragment")

    ctx.actions.run(
        inputs = depset(
            direct = [
                ctx.executable.base,
                ctx.executable.target,
                ctx.file.bloaty_config,
                script_input,
            ],
        ),
        progress_message = "Generating RST size report for " + ctx.label.name,
        executable = ctx.executable._bloat_script,
        arguments = [args],
        outputs = [rst_output],
    )

    return [
        DefaultInfo(files = depset([rst_output])),
        PwBloatInfo(fragment = rst_output),
    ]

pw_bloat_report = rule(
    implementation = _pw_bloat_report_impl,
    attrs = {
        "base": attr.label(
            executable = True,
            cfg = "target",
            allow_single_file = True,
            doc = "Optional base binary for a size diff report",
        ),
        "bloaty_config": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "Bloaty configuration file to use for the size report",
        ),
        "label": attr.string(doc = "Title for the size report"),
        "target": attr.label(
            mandatory = True,
            executable = True,
            cfg = "target",
            allow_single_file = True,
            doc = "Binary on which to run the size report",
        ),
        "_bloat_script": attr.label(
            default = Label("//pw_bloat/py:bloat_build"),
            executable = True,
            cfg = "exec",
        ),
    },
)

def pw_size_table_impl(ctx):
    """Implementation of the pw_size_table rule.

    Args:
      ctx: Rule execution context.

    Returns:
      DefaultInfo provider with the output RST file.
    """
    table_output = ctx.actions.declare_file(ctx.label.name)

    input_fragments = []

    for report in ctx.attr.reports:
        input_fragments.append(report[PwBloatInfo].fragment)

    args = ctx.actions.args()
    args.add("--collect-fragments-to={}".format(table_output.path))

    for fragment in input_fragments:
        args.add(fragment.path)

    ctx.actions.run(
        inputs = depset(direct = input_fragments),
        progress_message = "Generating RST report table for " + ctx.label.name,
        executable = ctx.executable._bloat_script,
        arguments = [args],
        outputs = [table_output],
    )

    return DefaultInfo(files = depset([table_output]))
