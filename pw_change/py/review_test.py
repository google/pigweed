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
"""Tests for pw_review.review."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

from pw_change import review


class ReviewTest(unittest.TestCase):
    """Tests for the review script."""

    def setUp(self) -> None:
        """Set up mocks and a temporary directory."""
        self.temp_dir = tempfile.TemporaryDirectory()
        self.project_root = Path(self.temp_dir.name)

        # Create a fake prompt file
        self.gemini_dir = self.project_root / '.gemini'
        self.gemini_dir.mkdir()
        self.prompt_file = self.gemini_dir / 'g-review_prompt.md'
        self.prompt_file.write_text('Test prompt')

    def tearDown(self) -> None:
        """Clean up the temporary directory."""
        self.temp_dir.cleanup()

    @patch('pw_cli.git_repo.GitRepo')
    @patch('subprocess.run')
    @patch('pw_cli.env.project_root')
    @patch('tempfile.NamedTemporaryFile')
    def test_review_success(
        self,
        mock_named_temp_file: MagicMock,
        mock_project_root: MagicMock,
        mock_subprocess_run: MagicMock,
        mock_git_repo: MagicMock,
    ) -> None:
        """Test a successful review command."""
        # Mock project_root to return our temporary directory
        mock_project_root.return_value = self.project_root

        # Mock GitRepo
        mock_repo_instance = mock_git_repo.return_value
        mock_repo_instance.commit_hash.return_value = '1234567'
        mock_repo_instance.commit_message.return_value = 'Test commit message'
        mock_repo_instance.show.return_value = 'file.txt\n-old\n+new'

        # Mock subprocess.run
        mock_proc = MagicMock()
        mock_proc.stdout = '```json\n{}```'.format(
            json.dumps(
                {
                    'response_text': 'This is a review.',
                    'diff': (
                        'diff --git a/file.txt b/file.txt\n'
                        '--- a/file.txt\n'
                        '+++ b/file.txt\n'
                        '@@ -1 +1 @@\n'
                        '-old\n'
                        '+new\n'
                    ),
                }
            )
        )
        mock_subprocess_run.return_value = mock_proc

        # Mock NamedTemporaryFile
        mock_temp_file = MagicMock()
        mock_temp_file.name = str(Path(self.temp_dir.name) / '1234567.patch')
        mock_named_temp_file.return_value = mock_temp_file

        # Run the review function
        review.review()

        # Assertions
        mock_subprocess_run.assert_called_once()
        mock_named_temp_file.assert_called_once_with(
            prefix='1234567', suffix='.patch', delete=False
        )
        # Check that the patch file was created
        patch_files = list(Path(self.temp_dir.name).glob('*.patch'))
        self.assertEqual(len(patch_files), 1)
        self.assertIn('1234567', patch_files[0].name)
        self.assertEqual(
            patch_files[0].read_text(),
            'diff --git a/file.txt b/file.txt\n'
            '--- a/file.txt\n'
            '+++ b/file.txt\n'
            '@@ -1 +1 @@\n'
            '-old\n'
            '+new\n',
        )


if __name__ == '__main__':
    unittest.main()
