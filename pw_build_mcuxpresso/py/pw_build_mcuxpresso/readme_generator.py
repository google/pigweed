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
"""README generation support"""

import shlex
import sys
from pathlib import Path

# pylint: disable=line-too-long
README_TEMPLATE = r'''
# MCUXpresso-SDK

This repository is auto-generated from the files from
[NXP SDK code](https://github.com/nxp-mcuxpresso/mcux-sdk) using
[pw_build_mcuxpresso](https://pigweed.dev/pw_build_mcuxpresso/) pigweed module.
The purpose of this repository is to be used as a module for
[Pigweed](https://pigweed.dev) applications targeting the NXP RT595 MCUs.
The repository includes bazel rules allowing seamless integration into
Pigweed's build system.

## Used Revisions

| Name | Revision | Source |
| ---- | -------- | ------ |
{sources}

## Running generation script locally

The script used to generate this repository is part of
[Pigweed](https://pigweed.dev).
To generate this repository locally, you will have to
[setup Pigweed](https://pigweed.dev/docs/get_started)
on your machine. After that, you can run this command to generate this
repository.

```sh
bazelisk run //pw_build_mcuxpresso/py:mcuxpresso_builder -- {args}
```

## License
Files taken from the NXP SDK repository are licensed under the BSD-3-Clause
license.
'''
# pylint: enable=line-too-long


def generate_readme(
    modules: list[tuple[str, str, str]],
    output_dir: Path,
    filename: str,
):
    """
    Generate SDK readme file

    Args:
        src_repo: source repository URL
        src_version: source repository version,
        modules: list of west modules
        output_dir: path to output directory,
        filename: name for generated file,
    """
    readme = output_dir / filename

    args = " \\\n  ".join(
        shlex.quote(arg) if " " in arg else arg for arg in sys.argv[1:]
    )

    sources = "\n".join(
        f"| {name} | [`{version}`]({url}/tree/{version}) | {url} |"
        for name, version, url in modules
    )

    with open(readme, "w") as f:
        f.write(
            README_TEMPLATE.format(
                args=args,
                sources=sources,
            )
        )
