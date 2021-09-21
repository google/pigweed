# Copyright 2021 The Pigweed Authors
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
"""Unit tests for pw_software_update/keygen.py."""

from pathlib import Path
import tempfile
import unittest

from pw_software_update.keygen import gen_ecdsa_sha2_nistp256_keypair
from securesystemslib.interface import (  # type: ignore
    import_ecdsa_privatekey_from_file, import_ecdsa_publickey_from_file)


class KeyGenTest(unittest.TestCase):
    """Test the generation of ECDSA keys."""
    def test_ecdsa_keygen(self):
        """Checks generated ECDSA keys are importable."""
        with tempfile.TemporaryDirectory() as tempdir_name:
            temp_root = Path(tempdir_name)

            private_key_filename = (temp_root / 'test_key')
            public_key_filename = (temp_root / 'test_key.pub')

            self.assertFalse(private_key_filename.exists())
            self.assertFalse(public_key_filename.exists())

            gen_ecdsa_sha2_nistp256_keypair(str(private_key_filename))

            self.assertTrue(private_key_filename.exists())
            self.assertTrue(public_key_filename.exists())

            private_key = import_ecdsa_privatekey_from_file(
                str(private_key_filename))
            self.assertEqual(private_key['scheme'], 'ecdsa-sha2-nistp256')

            public_key = import_ecdsa_publickey_from_file(
                str(public_key_filename))
            self.assertEqual(public_key['scheme'], 'ecdsa-sha2-nistp256')


if __name__ == '__main__':
    unittest.main()
