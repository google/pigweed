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

    maxDiff = None

    def test_missing_compatible(self) -> None:
        """Check that missing 'compatible' key throws exception"""
        self._check_with_exception(
            metadata={},
            exception_string="ERROR: Malformed sensor metadata YAML:\n{}",
            cause_substrings=["'compatible' is a required property"],
        )

    def test_invalid_compatible_type(self) -> None:
        """Check that incorrect type of 'compatible' throws exception"""
        self._check_with_exception(
            metadata={"compatible": {}, "supported-buses": ["i2c"]},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML:\ncompatible: {}\n"
                + "supported-buses:\n- i2c"
            ),
            cause_substrings=[
                "'part' is a required property",
            ],
        )

        self._check_with_exception(
            metadata={"compatible": [], "supported-buses": ["i2c"]},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML:\ncompatible: []\n"
                + "supported-buses:\n- i2c"
            ),
            cause_substrings=["[] is not of type 'object'"],
        )

        self._check_with_exception(
            metadata={"compatible": 1, "supported-buses": ["i2c"]},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML:\ncompatible: 1\n"
                + "supported-buses:\n- i2c"
            ),
            cause_substrings=["1 is not of type 'object'"],
        )

        self._check_with_exception(
            metadata={"compatible": "", "supported-buses": ["i2c"]},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML:\ncompatible: ''\n"
                + "supported-buses:\n- i2c"
            ),
            cause_substrings=[" is not of type 'object'"],
        )

    def test_partial_compatible_string(self) -> None:
        """
        Check that missing 'org' generates correct keys and empty entries are
        removed.
        """
        metadata: dict = {
            "compatible": {"part": "pigweed"},
            "supported-buses": ["i2c"],
        }
        result = Validator().validate(metadata=metadata)
        self.assertIn("pigweed", result["sensors"])
        self.assertDictEqual(
            {"part": "pigweed"},
            result["sensors"]["pigweed"]["compatible"],
        )

        metadata["compatible"]["org"] = " "
        result = Validator().validate(metadata=metadata)
        self.assertIn("pigweed", result["sensors"])
        self.assertDictEqual(
            {"part": "pigweed"},
            result["sensors"]["pigweed"]["compatible"],
        )

    def test_compatible_string_to_lower(self) -> None:
        """
        Check that compatible components are converted to lowercase and
        stripped.
        """
        metadata = {
            "compatible": {"org": "Google", "part": "Pigweed"},
            "supported-buses": ["i2c"],
        }
        result = Validator().validate(metadata=metadata)
        self.assertIn("google,pigweed", result["sensors"])
        self.assertDictEqual(
            {"org": "google", "part": "pigweed"},
            result["sensors"]["google,pigweed"]["compatible"],
        )

    def test_invalid_supported_buses(self) -> None:
        """
        Check that invalid or missing supported-buses cause an error
        """
        self._check_with_exception(
            metadata={"compatible": {"org": "Google", "part": "Pigweed"}},
            exception_string=(
                "ERROR: Malformed sensor metadata YAML:\ncompatible:\n"
                + "  org: Google\n  part: Pigweed"
            ),
            cause_substrings=[],
        )

        self._check_with_exception(
            metadata={
                "compatible": {"org": "Google", "part": "Pigweed"},
                "supported-buses": [],
            },
            exception_string=(
                "ERROR: Malformed sensor metadata YAML:\ncompatible:\n"
                + "  org: Google\n  part: Pigweed\nsupported-buses: []"
            ),
            cause_substrings=[],
        )

    def test_unique_bus_names(self) -> None:
        """
        Check that resulting bus names are unique and are converted to lowercase
        """
        self._check_with_exception(
            metadata={
                "compatible": {"org": "google", "part": "foo"},
                "supported-buses": ["i2c", "I2C", "SPI"],
                "deps": [],
            },
            exception_string=(
                "ERROR: bus list contains duplicates when converted to "
                "lowercase and concatenated with '_': "
                "['I2C', 'SPI', 'i2c'] -> ['i2c', 'spi']"
            ),
            cause_substrings=[],
        )
        self._check_with_exception(
            metadata={
                "compatible": {"org": "google", "part": "foo"},
                "supported-buses": ["i 2 c", "i  2_c", "i\t2-c"],
                "deps": [],
            },
            exception_string=(
                "ERROR: bus list contains duplicates when converted to "
                "lowercase and concatenated with '_': "
                "['i\\t2-c', 'i  2_c', 'i 2 c'] -> ['i_2_c']"
            ),
            cause_substrings=[],
        )

    def test_invalid_sensor_attribute(self) -> None:
        attribute = {
            "attribute": "sample_rate",
            "channel": "laundry",
            "trigger": "data_ready",
            "units": "rate",
        }
        dep_filename = self._generate_dependency_file()
        self._check_with_exception(
            metadata={
                "compatible": {"part": "foo"},
                "supported-buses": ["i2c"],
                "deps": [str(dep_filename.resolve())],
                "attributes": [attribute],
            },
            exception_string=(
                "Attribute instances cannot specify both channel AND trigger:\n"
                + yaml.safe_dump(attribute, indent=2)
            ),
            cause_substrings=[],
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
                    "supported-buses": ["i2c"],
                    "description": "",
                    "channels": {},
                    "attributes": [],
                    "triggers": [],
                    "extras": {},
                },
            },
            "channels": {},
            "attributes": {},
            "triggers": {},
            "units": {},
        }
        metadata = {
            "compatible": {"org": "google", "part": "foo"},
            "supported-buses": ["i2c"],
            "deps": [],
        }
        result = Validator().validate(metadata=metadata)
        self.assertEqual(result, expected)

        metadata = {
            "compatible": {"org": "google", "part": "foo"},
            "supported-buses": ["i2c"],
        }
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
                "supported-buses": ["i2c"],
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
                "supported-buses": ["i2c"],
                "channels": {"bar": []},
            },
            exception_string="Failed to find a definition for 'bar', did"
            " you forget a dependency?",
            cause_substrings=[],
        )

    @staticmethod
    def _generate_dependency_file() -> Path:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".yaml", encoding="utf-8", delete=False
        ) as dep:
            dep_filename = Path(dep.name)
            dep.write(
                yaml.safe_dump(
                    {
                        "units": {
                            "rate": {
                                "symbol": "Hz",
                            },
                            "sandwiches": {
                                "symbol": "sandwiches",
                            },
                            "squeaks": {"symbol": "squeaks"},
                            "items": {
                                "symbol": "items",
                            },
                        },
                        "attributes": {
                            "sample_rate": {},
                        },
                        "channels": {
                            "bar": {
                                "units": "sandwiches",
                            },
                            "soap": {
                                "name": "The soap",
                                "description": (
                                    "Measurement of how clean something is"
                                ),
                                "units": "squeaks",
                            },
                            "laundry": {
                                "description": "Clean clothes count",
                                "units": "items",
                            },
                        },
                        "triggers": {
                            "data_ready": {
                                "description": "notify when new data is ready",
                            },
                        },
                    },
                )
            )
        return dep_filename

    def test_channel_info_from_deps(self) -> None:
        """
        End to end test resolving a dependency file and setting the right
        default attribute values.
        """
        dep_filename = self._generate_dependency_file()

        metadata = Validator(include_paths=[dep_filename.parent]).validate(
            metadata={
                "compatible": {"org": "google", "part": "foo"},
                "supported-buses": ["i2c"],
                "deps": [dep_filename.name],
                "attributes": [
                    # Attribute applied to a channel
                    {
                        "attribute": "sample_rate",
                        "channel": "laundry",
                        "units": "rate",
                    },
                    # Attribute applied to the entire device
                    {
                        "attribute": "sample_rate",
                        "units": "rate",
                    },
                    # Attribute applied to a trigger
                    {
                        "attribute": "sample_rate",
                        "trigger": "data_ready",
                        "units": "rate",
                    },
                ],
                "channels": {
                    "bar": [],
                    "soap": [],
                    "laundry": [
                        {"name": "kids' laundry"},
                        {"name": "adults' laundry"},
                    ],
                },
                "triggers": [
                    "data_ready",
                ],
            },
        )

        # Check attributes
        self.assertEqual(
            metadata,
            {
                "attributes": {
                    "sample_rate": {
                        "name": "sample_rate",
                        "description": "",
                    },
                },
                "channels": {
                    "bar": {
                        "name": "bar",
                        "description": "",
                        "units": "sandwiches",
                    },
                    "soap": {
                        "name": "The soap",
                        "description": "Measurement of how clean something is",
                        "units": "squeaks",
                    },
                    "laundry": {
                        "name": "laundry",
                        "description": "Clean clothes count",
                        "units": "items",
                    },
                },
                "triggers": {
                    "data_ready": {
                        "name": "data_ready",
                        "description": "notify when new data is ready",
                    },
                },
                "units": {
                    "rate": {
                        "name": "Hz",
                        "symbol": "Hz",
                        "description": "",
                    },
                    "sandwiches": {
                        "name": "sandwiches",
                        "symbol": "sandwiches",
                        "description": "",
                    },
                    "squeaks": {
                        "name": "squeaks",
                        "symbol": "squeaks",
                        "description": "",
                    },
                    "items": {
                        "name": "items",
                        "symbol": "items",
                        "description": "",
                    },
                },
                "sensors": {
                    "google,foo": {
                        "description": "",
                        "compatible": {
                            "org": "google",
                            "part": "foo",
                        },
                        "supported-buses": ["i2c"],
                        "attributes": [
                            {
                                "attribute": "sample_rate",
                                "channel": "laundry",
                                "units": "rate",
                            },
                            {
                                "attribute": "sample_rate",
                                "units": "rate",
                            },
                            {
                                "attribute": "sample_rate",
                                "trigger": "data_ready",
                                "units": "rate",
                            },
                        ],
                        "channels": {
                            "bar": [
                                {
                                    "name": "bar",
                                    "description": "",
                                    "units": "sandwiches",
                                },
                            ],
                            "soap": [
                                {
                                    "name": "The soap",
                                    "description": (
                                        "Measurement of how clean something is"
                                    ),
                                    "units": "squeaks",
                                },
                            ],
                            "laundry": [
                                {
                                    "name": "kids' laundry",
                                    "description": "Clean clothes count",
                                    "units": "items",
                                },
                                {
                                    "name": "adults' laundry",
                                    "description": "Clean clothes count",
                                    "units": "items",
                                },
                            ],
                        },
                        "triggers": ["data_ready"],
                        "extras": {},
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

        self.assertEqual(
            str(context.exception).rstrip(), str(exception_string).rstrip()
        )
        for cause_substring in cause_substrings:
            self.assertTrue(
                cause_substring in str(context.exception.__cause__),
                f"Actual cause: {str(context.exception.__cause__)}",
            )


if __name__ == "__main__":
    unittest.main()
