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
"""Unit tests for the sensor metadata validator"""

from pathlib import Path
import unittest
import tempfile
import yaml
from pw_sensor.validator import Validator


class ValidatorTest(unittest.TestCase):
    """Tests the Validator class."""

    def test_missing_compatible(self) -> None:
        """Check that missing 'compatible' key throws exception"""
        self._check_with_exception(
            metadata={},
            exception_string="ERROR: Malformed sensor metadata YAML: {}",
            cause_substrings=["'compatible' is a required property"],
        )

    def test_invalid_compatible_type(self) -> None:
        """Check that incorrect type of 'compatible' throws exception"""
        self._check_with_exception(
            metadata={"compatible": {}},
            exception_string="ERROR: Malformed sensor metadata YAML: compatible: {}",
            cause_substrings=[
                "'org' is a required property",
            ],
        )

        self._check_with_exception(
            metadata={"compatible": []},
            exception_string="ERROR: Malformed sensor metadata YAML: compatible: []",
            cause_substrings=["[] is not of type 'object'"],
        )

        self._check_with_exception(
            metadata={"compatible": 1},
            exception_string="ERROR: Malformed sensor metadata YAML: compatible: 1",
            cause_substrings=["1 is not of type 'object'"],
        )

        self._check_with_exception(
            metadata={"compatible": ""},
            exception_string="ERROR: Malformed sensor metadata YAML: compatible: ''",
            cause_substrings=[" is not of type 'object'"],
        )

    def test_empty_dependency_list(self) -> None:
        """
        Check that an empty or missing 'deps' resolves to one with an empty
        'deps' list
        """
        expected = {
            "compatible": {"org": "google", "part": "foo"},
            "deps": [],
            "channels": {},
        }
        metadata = {
            "compatible": {"org": "google", "part": "foo"},
            "deps": [],
        }
        Validator().validate(metadata=metadata)
        self.assertEqual(metadata, expected)

        metadata = {"compatible": {"org": "google", "part": "foo"}}
        Validator().validate(metadata=metadata)
        self.assertEqual(metadata, expected)

    def test_invalid_dependency_file(self) -> None:
        """
        Check that if an invalid dependency file is listed, we throw an error.
        We know this will not be a valid file, because we have no files in the
        include path so we have nowhere to look for the file.
        """
        self._check_with_exception(
            metadata={
                "compatible": {"org": "google", "part": "foo"},
                "deps": ["test.yaml"],
            },
            exception_string="Failed to find test.yaml using search paths:",
            cause_substrings=[],
            exception_type=FileNotFoundError,
        )

    def test_invalid_channel_name_raises_exception(self) -> None:
        """
        Check that if given a channel name that's not defined, we raise an Error
        """
        self._check_with_exception(
            metadata={
                "compatible": {"org": "google", "part": "foo"},
                "channels": {"bar": {}},
            },
            exception_string="Failed to find a channel defenition for bar, did"
            " you forget a dependency?",
            cause_substrings=[],
        )

    def test_channel_info_from_deps(self) -> None:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".yaml", encoding="utf-8", delete=False
        ) as dep:
            dep_filename = Path(dep.name)
            dep.write(
                yaml.safe_dump(
                    {
                        "channels": {
                            "bar": {
                                "units": {"symbol": "sandwiches"},
                            },
                            "soap": {
                                "name": "The soap",
                                "description": "Measurement of how clean something is",
                                "units": {"symbol": "sqeaks"},
                            },
                            "laundry": {
                                "description": "Clean clothes count",
                                "units": {"symbol": "items"},
                                "sub-channels": {
                                    "shirts": {
                                        "description": "Clean shirt count",
                                    },
                                    "pants": {
                                        "description": "Clean pants count",
                                    },
                                },
                            },
                        },
                    },
                )
            )

        metadata = Validator(include_paths=[dep_filename.parent]).validate(
            metadata={
                "compatible": {"org": "google", "part": "foo"},
                "deps": [dep_filename.name],
                "channels": {
                    "bar": {},
                    "soap": {
                        "name": "soap name override",
                    },
                    "laundry_shirts": {},
                    "laundry_pants": {},
                    "laundry": {
                        "indicies": [
                            {"name": "kids' laundry"},
                            {"name": "adults' laundry"},
                        ]
                    },
                },
            }
        )
        self.assertEqual(
            metadata["channels"]["bar"],
            {
                "name": "bar",
                "description": "",
                "units": {
                    "name": "sandwiches",
                    "symbol": "sandwiches",
                },
                "indicies": [
                    {
                        "name": "bar",
                        "description": "",
                    },
                ],
            },
        )
        self.assertEqual(
            metadata["channels"]["soap"],
            {
                "name": "soap name override",
                "description": "Measurement of how clean something is",
                "units": {
                    "name": "sqeaks",
                    "symbol": "sqeaks",
                },
                "indicies": [
                    {
                        "name": "soap name override",
                        "description": "Measurement of how clean something is",
                    },
                ],
            },
        )
        self.assertEqual(
            metadata["channels"]["laundry_shirts"],
            {
                "name": "laundry_shirts",
                "description": "Clean shirt count",
                "units": {
                    "name": "items",
                    "symbol": "items",
                },
                "indicies": [
                    {
                        "name": "laundry_shirts",
                        "description": "Clean shirt count",
                    },
                ],
            },
        )
        self.assertEqual(
            metadata["channels"]["laundry_pants"],
            {
                "name": "laundry_pants",
                "description": "Clean pants count",
                "units": {
                    "name": "items",
                    "symbol": "items",
                },
                "indicies": [
                    {
                        "name": "laundry_pants",
                        "description": "Clean pants count",
                    },
                ],
            },
        )
        self.assertEqual(
            metadata["channels"]["laundry"],
            {
                "name": "laundry",
                "description": "Clean clothes count",
                "units": {
                    "name": "items",
                    "symbol": "items",
                },
                "indicies": [
                    {
                        "name": "kids' laundry",
                        "description": "Clean clothes count",
                    },
                    {
                        "name": "adults' laundry",
                        "description": "Clean clothes count",
                    },
                ],
            },
        )

    def _check_with_exception(
        self,
        metadata: dict,
        exception_string: str,
        cause_substrings: list[str],
        exception_type: type[BaseException] = RuntimeError,
    ) -> None:
        with self.assertRaises(exception_type) as context:
            Validator().validate(metadata=metadata)

        self.assertEqual(str(context.exception).rstrip(), exception_string)
        for cause_substring in cause_substrings:
            self.assertTrue(
                cause_substring in str(context.exception.__cause__),
                f"Actual cause: {str(context.exception.__cause__)}",
            )


if __name__ == "__main__":
    unittest.main()
