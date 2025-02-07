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
"""Defines a rule for collecting size reports into a single table."""

load("//pw_bloat/private:pw_bloat_report.bzl", _pw_size_table_impl = "pw_size_table_impl")

pw_size_table = rule(
    implementation = _pw_size_table_impl,
    attrs = {
        "reports": attr.label_list(),
        "_bloat_script": attr.label(
            default = Label("//pw_bloat/py:bloat_build"),
            executable = True,
            cfg = "exec",
        ),
    },
)
