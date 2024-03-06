// Copyright 2024 The Pigweed Authors
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
#[cfg(test)]
mod tests {
    use crate::run_with_capture;
    use pw_log_backend::pw_logf_backend;
    use pw_log_backend_api::LogLevel;

    #[test]
    fn no_argument_log_line_prints_to_stdout() {
        assert_eq!(
            run_with_capture(|| pw_logf_backend!(LogLevel::Info, "test")),
            "[INF] test\n"
        );
    }

    #[test]
    fn integer_argument_prints_to_stdout() {
        assert_eq!(
            run_with_capture(|| pw_logf_backend!(LogLevel::Info, "test %d", -1)),
            "[INF] test -1\n",
        );
    }

    #[test]
    fn unsigned_argument_prints_to_stdout() {
        assert_eq!(
            run_with_capture(|| pw_logf_backend!(LogLevel::Info, "test %u", 1u32)),
            "[INF] test 1\n",
        );
    }

    #[test]
    fn string_argument_prints_to_stdout() {
        assert_eq!(
            run_with_capture(|| pw_logf_backend!(LogLevel::Info, "test %s", "test")),
            "[INF] test test\n",
        );
    }

    #[test]
    fn character_argument_prints_to_stdout() {
        assert_eq!(
            run_with_capture(|| pw_logf_backend!(LogLevel::Info, "test %c", 'c')),
            "[INF] test c\n",
        );
    }
}
