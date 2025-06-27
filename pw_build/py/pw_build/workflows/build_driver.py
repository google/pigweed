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
"""An interface for defining Workflows build drivers."""

import abc
import argparse
import json
import sys

from google.protobuf import json_format
from pw_build.proto import build_driver_pb2


class BuildDriver(abc.ABC):
    """A Workflows build driver interface for drivers written in Python."""

    @abc.abstractmethod
    def generate_action_sequence(
        self, job: build_driver_pb2.JobRequest
    ) -> build_driver_pb2.JobResponse:
        """Generates an action sequence from a single job request."""

    def generate_jobs(
        self, build_request: build_driver_pb2.BuildDriverRequest
    ) -> build_driver_pb2.BuildDriverResponse:
        return build_driver_pb2.BuildDriverResponse(
            jobs=[
                self.generate_action_sequence(job) for job in build_request.jobs
            ]
        )

    def generate_jobs_from_json(
        self, json_msg: str
    ) -> build_driver_pb2.BuildDriverResponse:
        json_msg = json.loads(json_msg)
        msg = build_driver_pb2.BuildDriverRequest()
        json_format.ParseDict(json_msg, msg)
        return self.generate_jobs(msg)

    @staticmethod
    def _parse_args():
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '--request',
            default=sys.stdin,
            type=argparse.FileType('r'),
        )
        return parser.parse_args()

    def main(self) -> int:
        args = self._parse_args()
        response = self.generate_jobs_from_json(args.request.read())

        print(
            json_format.MessageToJson(
                response,
                preserving_proto_field_name=True,
            )
        )
        return 0
