#!/usr/bin/env python3
# Copyright 2023 The Pigweed Authors
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
"""Tests for presubmit context classes."""

import unittest

from pw_presubmit import presubmit_context


class ContextTest(unittest.TestCase):
    """Tests for presubmit context classes."""

    def test_presubmitcontext(self):  # pylint: disable=no-self-use
        _ = presubmit_context.PresubmitContext.create_for_testing()

    def test_lucicontext(self):
        ctx = presubmit_context.LuciContext.create_for_testing(
            BUILDBUCKET_ID='88123',
            BUILDBUCKET_NAME='project:bucket.dev.ci:builder-linux',
            BUILD_NUMBER='12',
            SWARMING_SERVER='https://chrome-swarming.appspot.com',
            SWARMING_TASK_ID='123def',
        )

        self.assertEqual(ctx.buildbucket_id, 88123)
        self.assertEqual(ctx.build_number, 12)
        self.assertEqual(ctx.project, 'project')
        self.assertEqual(ctx.bucket, 'bucket.dev.ci')
        self.assertEqual(ctx.builder, 'builder-linux')
        self.assertEqual(
            ctx.swarming_server,
            'https://chrome-swarming.appspot.com',
        )
        self.assertEqual(ctx.swarming_task_id, '123def')
        self.assertEqual(
            ctx.cas_instance,
            'projects/chrome-swarming/instances/default_instance',
        )

        self.assertFalse(ctx.is_try)
        self.assertTrue(ctx.is_ci)
        self.assertTrue(ctx.is_dev)
        self.assertFalse(ctx.is_shadow)
        self.assertFalse(ctx.is_prod)

    def test_lucitrigger(self):
        trigger = presubmit_context.LuciTrigger.create_for_testing(
            number=1234,
            patchset=5,
            remote='https://pigweed-internal.googlesource.com/pigweed',
            project='pigweed',
            branch='main',
            ref='refs/changes/34/1234/5',
            gerrit_name='pigweed-internal',
            submitted=False,
        )[0]

        self.assertEqual(
            trigger.gerrit_host,
            'https://pigweed-internal-review.googlesource.com',
        )
        self.assertEqual(
            trigger.gerrit_url,
            'https://pigweed-internal-review.googlesource.com/c/1234',
        )
        self.assertEqual(
            trigger.gitiles_url,
            'https://pigweed-internal.googlesource.com/pigweed/+/'
            'refs/changes/34/1234/5',
        )


if __name__ == '__main__':
    unittest.main()
