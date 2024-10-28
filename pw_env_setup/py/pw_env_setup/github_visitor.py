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
# pylint: disable=line-too-long
"""Serializes an Environment into files in a way GitHub Actions understands.

See also:

* https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions#setting-an-environment-variable
* https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions#adding-a-system-path
"""
# pylint: enable=line-too-long

import contextlib
import os


class GitHubVisitor:
    """Serializes an Environment into files GitHub Actions understands."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._replacements = ()
        self._github_env = None
        self._github_path = None
        self._log = None

    def serialize(self, env, root):
        """Write a shell file based on the given environment.

        Args:
            env (environment.Environment): Environment variables to use.
            outs (file): Shell file to write.
        """
        try:
            self._replacements = tuple(
                (key, env.get(key) if value is None else value)
                for key, value in env.replacements
            )

            with contextlib.ExitStack() as stack:
                github_env = os.environ.get('GITHUB_ENV')
                mode = 'a'
                if not github_env:
                    github_env = os.path.join(root, 'github_env.log')
                    mode = 'w'
                self._github_env = stack.enter_context(open(github_env, mode))

                github_path = os.environ.get('GITHUB_PATH')
                mode = 'a'
                if not github_path:
                    github_path = os.path.join(root, 'github_path.log')
                    mode = 'w'
                self._github_path = stack.enter_context(open(github_path, mode))

                self._log = stack.enter_context(
                    open(os.path.join(root, 'github.log'), 'w')
                )

                env.accept(self)

        finally:
            self._replacements = ()
            self._github_env = None
            self._github_path = None
            self._log = None

    def visit_set(self, set):  # pylint: disable=redefined-builtin
        if '\n' in set.value:
            eof = '__EOF__'
            assert '\n{}\n'.format(eof) not in set.value
            print(
                '{name}=<<{eof}\n{value}\n{eof}'.format(
                    name=set.name,
                    value=set.value,
                    eof=eof,
                ),
                file=self._github_env,
            )
        else:
            print(
                '{name}={value}'.format(name=set.name, value=set.value),
                file=self._github_env,
            )
        print(
            'setting {name!r} = {value!r}'.format(
                name=set.name, value=set.value
            ),
            file=self._log,
        )

    def visit_clear(self, clear):
        print('{}='.format(clear.name), file=self._github_env)
        print('setting {!r} = ""'.format(clear.name), file=self._log)

    def visit_prepend(self, prepend):
        if prepend.name == 'PATH':
            print(prepend.value, file=self._github_path)
            print('adding {!r} to PATH'.format(prepend.value), file=self._log)
        else:
            print(
                'unsupported prepend: {name!r} += {value!r}'.format(
                    name=prepend.name, value=prepend.value
                ),
                file=self._log,
            )

    def visit_append(self, append):
        print(
            'unsupported append: {name!r} += {value!r}'.format(
                name=append.name, value=append.value
            ),
            file=self._log,
        )

    def visit_remove(self, remove):
        pass

    def visit_echo(self, echo):
        pass

    def visit_comment(self, comment):
        pass

    def visit_command(self, command):
        pass

    def visit_doctor(self, doctor):
        pass

    def visit_blank_line(self, blank_line):
        pass

    def visit_function(self, function):
        pass
