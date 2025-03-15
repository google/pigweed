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
"""Bazel related checks."""

from . import build, git_repo, presubmit, presubmit_context


def includes_presubmit_check(
    targets: str = '//...',
) -> presubmit.Check:
    """Presubmit check verifying cc_library targets don't have includes.

    See https://pwbug.dev/378564135 for a discussion of why the includes
    attribute should be avoided.

    Args:
      targets: A Bazel target pattern
        (https://bazel.build/run/build#specifying-build-targets).
    """

    @presubmit.filter_paths(endswith=('.bazel', '.bazelrc', '.bzl'))
    @presubmit.check(name='bazel_no_includes')
    def includes_check(
        ctx: presubmit_context.PresubmitContext,
    ):
        # This query returns the subset of `targets` which,
        #
        # * are of type cc_library, and
        # * have a nonempty list as the `includes` attribute.
        #
        # `bazel query` will print the targets matching these conditions, one
        # per line.
        query = r'attr("includes", ".{3,}", kind(cc_library, ' + targets + '))'

        # Ideally we would just use a io.StringIO here instead of creating the
        # files, but build.bazel doesn't actually accept _any_ TextIO object as
        # stdout. It requires an io.TextIOWrapper, which must have a .name
        # attribute that io.StringIO doesn't possess.
        ctx.output_dir.mkdir(exist_ok=True, parents=True)
        stdout_path = ctx.output_dir / 'bazel_includes.stdout'
        stderr_path = ctx.output_dir / 'bazel_includes.stderr'
        with open(stdout_path, 'w') as stdout, open(stderr_path, 'w') as stderr:
            build.bazel(ctx, 'query', query, stdout=stdout, stderr=stderr)

        with open(stdout_path, 'r') as stdout:
            contents = stdout.readlines()

        if not contents:
            # The check has passed: the query returned no targets.
            return

        with open(ctx.failure_summary_log, 'w') as outs:
            print(
                'The following cc_library targets contain the `includes` '
                'attribute. This attribute is forbidden. Use '
                '`strip_include_prefix` instead.',
                file=outs,
            )
            print('', file=outs)
            for target in contents:
                print(target.strip(), file=outs)

        print(ctx.failure_summary_log.read_text(), end=None)
        raise presubmit_context.PresubmitFailure(
            'cc_library targets contain includes'
        )

    return includes_check


def lockfile_check(ctx: presubmit_context.PresubmitContext) -> None:
    """Verify that MODULE.bazel.lock is up to date.

    Note: unlike most presubmit checks, this check intentionally modifies the
    contents of the working directory. It is intended for producing
    MODULE.bazel.lock updates for different host platforms on CI machines.
    See https://pigweed.dev/docs/infra/bazel_lockfile.html.

    Args:
      ctx: A presubmit context.
    """
    build.bazel(ctx, 'mod', 'deps', '--lockfile_mode=update')

    diff = git_repo.find_git_repo(ctx.root).diff('MODULE.bazel.lock')
    if not diff:
        return

    ctx.output_dir.mkdir(exist_ok=True, parents=True)
    diff_path = ctx.output_dir / 'git_diff.txt'
    with open(diff_path, 'w') as stdout:
        stdout.write(diff)

    raise presubmit_context.PresubmitFailure(
        f'git diff output is not empty; see {diff_path}'
    )
