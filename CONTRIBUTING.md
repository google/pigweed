# Contributing

We'd love to accept your patches and contributions to Pigweed. There are just a
few small guidelines you need to follow. Before making or sending major changes,
please reach out on the [mailing list](mailto:pigweed@googlegroups.com) first to
ensure the changes make sense for upstream. We generally go through a design
phase before making large changes.

Pigweed contribution overview:
 1. One-time contributor setup:
   * Sign the [Contributor License
     Agreement](https://cla.developers.google.com/).
   * Install the [gerrit commit hook](CONTRIBUTING.md#gerrit-commit-hook) to
     automatically add a Change-ID to your CL.
   * Install the Pigweed presubmit check hook (`pw presubmit --install`).
     (recommended)
 2. Ensure all files include a correct [copyright+license
    header](CONTRIBUTING.md#source-code-headers).
 2. Upload the change with `git push origin HEAD:refs/for/master`.
 3. Address any reviewer feedback by amending the commit (`git commit --amend`)
 4. Submit change to CI builders to merge.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Gerrit Commit Hook

Gerrit requires all changes to have a Change-ID tag at the bottom of each CL.
You should set this up to be done automatically using the instructions below.

**Linux/macOS**<br/>
```bash
$ f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f
```

**Windows**<br/>
Download [the Gerrit commit hook](https://gerrit-review.googlesource.com/tools/hooks/commit-msg)
and then copy it to the `.git\hooks` directory in the Pigweed repository.
```batch
copy %HOMEPATH%\Downloads\commit-msg %HOMEPATH%\pigweed\.git\hooks\commit-msg
```

## Code Reviews

All Pigweed development happens on Gerrit, following the [typical Gerrit
development workflow](http://ceres-solver.org/contributing.html).

In the future we may support GitHub pull requests, but until that time we will
close GitHub pull requests and ask that the changes be uploaded to Gerrit
instead.

## Community Guidelines

This project follows [Google's Open Source Community
Guidelines](https://opensource.google/conduct/) and the [Pigweed Code of
Conduct](CODE_OF_CONDUCT.md).

## Source Code Headers

Every Pigweed file containing source code must include copyright and license
information. This includes any JS/CSS files that you might be serving out to
browsers.

Apache header for C and C++ files:

```javascript
// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
```

Apache header for Python and GN files:

```python
# Copyright 2019 The Pigweed Authors
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
```

## Continuous Integration

All Pigweed CLs must adhere to Pigweed's style guide and pass a suite of
automated builds, tests, and style checks to be merged upstream. Much of this
checking is done using Pigweed's pw_presubmit module by automated builders. To
speed up the review process, consider adding `pw presubmit` as a git push hook
using the following command:

**Linux/macOS**<br/>
```bash
$ pw presubmit --install
```

This will be effectively the same as running the following command before every
`git push`:
```bash
$ pw presubmit --program quick
```

![pigweed presubmit demonstration](docs/images/pw_presubmit_demo.gif)

Running `pw presubmit` manually will default to running the `full` presubmit
program.

If you ever need to bypass the presubmit hook (due to it being broken, for example) you
may push using this command:

```bash
$ git push origin HEAD:refs/for/master --no-verify
```
