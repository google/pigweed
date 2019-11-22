#!/usr/bin/env python3

# Copyright 2019 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy
# of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

import os
import subprocess

from pw_presubmit.presubmit_tools import *

BUILDTOOLS_GIT = 'https://pigweed.googlesource.com/infra/buildtools'


def init():
    buildtools = '.presubmit/buildtools'
    if os.path.isdir(buildtools):
        call('git', 'fetch', BUILDTOOLS_GIT, 'master', cwd=buildtools)
        call('git', 'reset', '--hard', 'FETCH_HEAD', cwd=buildtools)
    else:
        call('git', 'clone', BUILDTOOLS_GIT, buildtools)

    call(os.path.join(buildtools, 'update.py'))
    os.environ['PATH'] = os.pathsep.join((
        os.path.join(buildtools, 'tools'),
        os.path.join(buildtools, 'tools', 'bin'),
        os.environ['PATH'],
    ))
    print('PATH', os.environ['PATH'])


def gn_test():
    """Test with gn."""
    out = '.presubmit/gn'
    call('gn', 'gen', '--check', out)
    call('ninja', '-C', out)


def bazel_test():
    """Test with bazel."""
    prefix = '.presubmit/bazel-'
    call('bazel', 'build', '//...', '--symlink_prefix', prefix)
    call('bazel', 'test', '//...', '--symlink_prefix', prefix)


@filter_paths(endswith=['.gn', '.gni'])
def gn_format(paths):
    call('gn', 'format', '--dry-run', *paths)


@filter_paths(endswith='.py')
def pylint(paths):
    call(sys.executable, '-m', 'pylint', '-E', *paths)


PRESUBMIT_PROGRAM = (
  init,
  pragma_once,
  gn_format,
  # pylint,  # TODO(hepler): enable pylint when it passes
  bazel_test,
  gn_test,
)



if __name__ == '__main__':
    sys.exit(0 if parse_args_and_run_presubmit(PRESUBMIT_PROGRAM) else 1)
