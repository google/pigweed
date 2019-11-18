#!/usr/bin/env python3

import os
import subprocess

BUILDTOOLS_GIT = 'https://pigweed.googlesource.com/infra/buildtools'


def call(*args, **kwargs):
    print()
    for k, v in sorted(kwargs.items()):
        print('#', k, '=', v)
    print('$', *args)
    subprocess.check_call(args=args, **kwargs)


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
    call('gn', 'gen', out)
    call('ninja', '-C', out)


def bazel_test():
    """Test with bazel."""
    prefix = '.presubmit/bazel-'
    call('bazel', 'build', '//...', '--symlink_prefix', prefix)
    call('bazel', 'test', '//...', '--symlink_prefix', prefix)


if __name__ == '__main__':
    init()
    bazel_test()
    gn_test()
