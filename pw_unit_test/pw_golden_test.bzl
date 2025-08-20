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
"""pw_golden_test compares an executable's output to a golden file."""

load("//pw_build:compatibility.bzl", "incompatible_with_mcu")
load("//pw_build:python.bzl", "pw_py_binary", "pw_py_test")

def pw_golden_test(name, executable, expected, args = None, **kwargs):
    """Runs an executable and compares its output to a file.

    This macro creates two targets:
        $name: pw_py_test that runs the golden_test.py script
        $name.accept: pw_py_binary that updates the golden file

    Args:
        name: The name of the test target.
        executable: The executable target to run.
        expected: The file target with the expected output.
        args: Additional arguments to pass to the executable.
        **kwargs: Additional arguments for the underlying pw_py_test and
            pw_py_binary.
    """
    if args == None:
        args = []
    else:
        args = ["--"] + args

    pw_py_test(
        name = name,
        main = "pw_unit_test/py/pw_unit_test/golden_test.py",
        srcs = ["//pw_unit_test/py:pw_unit_test/golden_test.py"],
        args = [
            "--executable",
            "$(rootpath " + executable + ")",
            "--expected",
            "$(rootpath " + expected + ")",
            "--update-command",
            "\"bazelisk run //%s:%s.accept\"" % (native.package_name(), name),
        ] + args,
        data = [
            executable,
            expected,
        ],
        target_compatible_with = incompatible_with_mcu(),
        **kwargs
    )

    pw_py_binary(
        name = name + ".accept",
        main = "pw_unit_test/py/pw_unit_test/golden_test.py",
        srcs = ["//pw_unit_test/py:pw_unit_test/golden_test.py"],
        testonly = True,
        args = [
            "--executable",
            "$(execpath " + executable + ")",
            "--expected",
            "$(execpath " + expected + ")",
            "--accept",
        ] + args,
        data = [
            executable,
            expected,
        ],
        **kwargs
    )
