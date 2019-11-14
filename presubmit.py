#!/usr/bin/env python

from __future__ import print_function

import os
import subprocess


def call(*cmd):
    print()
    print('$', *cmd)
    subprocess.check_call(cmd)


def gn_test():
    """Test with gn."""
    out = '.presubmit/gn'
    call('gn', 'gen', out)

    with open(os.path.join(out, 'args.gn'), 'a') as outs:
        print('pw_unit_test_create_run_targets = true', file=outs)

    call('ninja', '-C', out)


def bazel_test():
    """Test with bazel."""
    prefix = '.presubmit/bazel-'
    call('bazel', 'build', '//...', '--symlink_prefix', prefix)
    call('bazel', 'test', '//...', '--symlink_prefix', prefix)


if __name__ == '__main__':
    bazel_test()
    gn_test()
