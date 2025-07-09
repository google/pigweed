// Copyright 2025 The Pigweed Authors
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

use std::env;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::process::Command;

// Simple utility to run commands from files.  Each command/command line
// argument in the file is on a separate line.
pub fn main() {
    // Skip the first arg (our executable path).
    let scripts = env::args().skip(1);

    for script in scripts {
        let script_file = BufReader::new(
            File::open(&script).unwrap_or_else(|_| panic!("{} should contain a command", script)),
        );
        let mut lines = script_file
            .lines()
            .map(|l| l.expect("Should be able to read lines from {script}"));

        // First line of the file is the command to run.
        let command = lines
            .next()
            .unwrap_or_else(|| panic!("{} should contain a command", script));

        // Subsequent lines are arguments.
        let args = lines.collect::<Vec<_>>();

        // Run the command and wait for it to finish.
        let mut child = Command::new(&command)
            .args(args)
            .spawn()
            .unwrap_or_else(|_| panic!("{} should spawn a child process", command));
        let status = child
            .wait()
            .expect("child process should complete with no error");

        assert!(
            status.success(),
            "{}: error running script: {}",
            script,
            status
        );
    }
}
