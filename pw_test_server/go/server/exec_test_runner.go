// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

package server

import (
	"fmt"
	"log"
	"os"
	"os/exec"

	pb "pigweed.dev/module/pw_test_server/gen"
)

// ExecTestRunner is a struct that implements the UnitTestRunner interface,
// running its tests by executing a command with the path of the unit test
// executable as an argument.
type ExecTestRunner struct {
	command []string
	logger  *log.Logger
}

// NewExecTestRunner creates a new ExecTestRunner with a custom logger.
func NewExecTestRunner(id int, command []string) *ExecTestRunner {
	logPrefix := fmt.Sprintf("[ExecTestRunner %d] ", id)
	logger := log.New(os.Stdout, logPrefix, log.LstdFlags)
	return &ExecTestRunner{command, logger}
}

// WorkerStart starts the worker. Part of UnitTestRunner interface.
func (r *ExecTestRunner) WorkerStart() error {
	r.logger.Printf("Starting worker")
	return nil
}

// WorkerExit exits the worker. Part of UnitTestRunner interface.
func (r *ExecTestRunner) WorkerExit() {
	r.logger.Printf("Exiting worker")
}

// HandleRunRequest runs a requested unit test binary by executing the runner's
// command with the unit test as an argument. The combined stdout and stderr of
// the command is returned as the unit test output.
func (r *ExecTestRunner) HandleRunRequest(req *UnitTestRunRequest) *UnitTestRunResponse {
	res := &UnitTestRunResponse{Status: pb.TestStatus_SUCCESS}

	r.logger.Printf("Running unit test %s\n", req.Path)

	// Copy runner command args, appending unit test binary path to the end.
	args := append([]string(nil), r.command[1:]...)
	args = append(args, req.Path)

	cmd := exec.Command(r.command[0], args...)
	output, err := cmd.CombinedOutput()

	if err != nil {
		if e, ok := err.(*exec.ExitError); ok {
			// A nonzero exit status is interpreted as a unit test
			// failure.
			r.logger.Printf("Command exited with status %d\n", e.ExitCode())
			res.Status = pb.TestStatus_FAILURE
		} else {
			// Any other error with the command execution is
			// reported as an internal error to the requester.
			r.logger.Printf("Command failed: %v\n", err)
			res.Err = err
			return res
		}
	}

	res.Output = output
	return res
}
