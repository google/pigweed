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

    def __init__(self, methodName: str = "runTest") -> None:
        super().__init__(methodName)
        self.maxDiff = None  # pylint: disable=invalid-name

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
            exception_string=(
                "ERROR: Malformed sensor metadata YAML: compatible: {}"
            ),
            cause_substrings=[
                "'org' is a required property",
            ],
        )

        self._check_with_exception(
            metadata={"compatible": []},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML: compatible: []"
            ),
            cause_substrings=["[] is not of type 'object'"],
        )

        self._check_with_exception(
            metadata={"compatible": 1},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML: compatible: 1"
            ),
            cause_substrings=["1 is not of type 'object'"],
        )

        self._check_with_exception(
            metadata={"compatible": ""},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML: compatible: ''"
            ),
            cause_substrings=[" is not of type 'object'"],
        )

    def test_empty_dependency_list(self) -> None:
        """
        Check that an empty or missing 'deps' resolves to one with an empty
        'deps' list
        """
        expected = {
            "sensors": {
                "google,foo": {
                    "compatible": {"org": "google", "part": "foo"},
                    "channels": {},
                    "attributes": {},
                },
            },
            "channels": {},
            "attributes": {},
        }
        metadata = {
            "compatible": {"org": "google", "part": "foo"},
            "deps": [],
        }
        result = Validator().validate(metadata=metadata)
        self.assertEqual(result, expected)

        metadata = {"compatible": {"org": "google", "part": "foo"}}
        result = Validator().validate(metadata=metadata)
        self.assertEqual(result, expected)

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
            exception_string="Failed to find a definition for 'bar', did"
            " you forget a dependency?",
            cause_substrings=[],
        )

    def test_channel_info_from_deps(self) -> None:
        """
        End to end test resolving a dependency file and setting the right
        default attribute values.
        """
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".yaml", encoding="utf-8", delete=False
        ) as dep:
            dep_filename = Path(dep.name)
            dep.write(
                yaml.safe_dump(
                    {
                        "attributes": {
                            "sample_rate": {
                                "units": {"symbol": "Hz"},
                            },
                        },
                        "channels": {
                            "bar": {
                                "units": {"symbol": "sandwiches"},
                            },
                            "soap": {
                                "name": "The soap",
                                "description": (
                                    "Measurement of how clean something is"
                                ),
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
                "attributes": {
                    "sample_rate": {},
                },
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
        expected_attribute_sample_rate = {
            "name": "sample_rate",
            "description": "",
            "units": {"name": "Hz", "symbol": "Hz"},
        }
        expected_channel_bar = {
            "name": "bar",
            "description": "",
            "units": {
                "name": "sandwiches",
                "symbol": "sandwiches",
            },
        }
        expected_channel_soap = {
            "name": "The soap",
            "description": "Measurement of how clean something is",
            "units": {
                "name": "sqeaks",
                "symbol": "sqeaks",
            },
        }
        expected_channel_laundry_shirts = {
            "name": "laundry_shirts",
            "description": "Clean shirt count",
            "units": {
                "name": "items",
                "symbol": "items",
            },
        }
        expected_channel_laundry_pants = {
            "name": "laundry_pants",
            "description": "Clean pants count",
            "units": {
                "name": "items",
                "symbol": "items",
            },
        }
        expected_channel_laundry = {
            "name": "laundry",
            "description": "Clean clothes count",
            "units": {
                "name": "items",
                "symbol": "items",
            },
        }
        expected_sensor_channel_bar = {
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
        }
        expected_sensor_channel_soap = {
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
        }
        expected_sensor_channel_laundry_shirts = {
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
        }
        expected_sensor_channel_laundry_pants = {
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
        }
        expected_sensor_channel_laundry = {
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
        }
        self.assertEqual(
            metadata,
            {
                "attributes": {"sample_rate": expected_attribute_sample_rate},
                "channels": {
                    "bar": expected_channel_bar,
                    "soap": expected_channel_soap,
                    "laundry_shirts": expected_channel_laundry_shirts,
                    "laundry_pants": expected_channel_laundry_pants,
                    "laundry": expected_channel_laundry,
                },
                "sensors": {
                    "google,foo": {
                        "compatible": {
                            "org": "google",
                            "part": "foo",
                        },
                        "attributes": {
                            "sample_rate": expected_attribute_sample_rate,
                        },
                        "channels": {
                            "bar": expected_sensor_channel_bar,
                            "soap": expected_sensor_channel_soap,
                            "laundry_shirts": (
                                expected_sensor_channel_laundry_shirts
                            ),
                            "laundry_pants": (
                                expected_sensor_channel_laundry_pants
                            ),
                            "laundry": expected_sensor_channel_laundry,
                        },
                    },
                },
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
