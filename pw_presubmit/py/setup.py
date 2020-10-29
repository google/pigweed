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
"""The pw_presubmit package."""

import setuptools  # type: ignore

setuptools.setup(
    name='pw_presubmit',
    version='0.0.1',
    author='Pigweed Authors',
    author_email='pigweed-developers@googlegroups.com',
    description='Presubmit tools and a presubmit script for Pigweed',
    install_requires=[
        'mypy==0.790',
        'pylint==2.6.0',
        'yapf==0.30.0',
        'pw_package',
    ],
    packages=setuptools.find_packages(),
    package_data={'pw_presubmit': ['py.typed']},
    zip_safe=False,
)
