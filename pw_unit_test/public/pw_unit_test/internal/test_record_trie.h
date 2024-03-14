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
#pragma once

#include <cassert>
#include <filesystem>
#include <unordered_map>

#include "pw_assert/check.h"
#include "pw_json/builder.h"
#include "pw_unit_test/event_handler.h"

namespace pw::unit_test::json_impl {

// Version of the JSON Test Result Format. Format can be found at
// https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/testing/json_test_results_format.md
inline constexpr int kJsonTestResultsFormatVersion = 3;

/// A class that records test results as a trie, or prefix tree, and is capable
/// of outputting the trie as a json string. The trie is structured as a
/// hierarchical format to reduce duplication of test suite names.
class TestRecordTrie {
 public:
  /// Constructor that initializes the root test record trie node.
  TestRecordTrie() {
    root_ = new TestRecordTrieNode();
    root_->prefix = "test_results";
  }

  /// Destructor that deletes all the allocated memory for the test record trie.
  ~TestRecordTrie() { DeleteTestRecordTrie(root_); }

  /// Adds a test result into the trie, creating new trie nodes if needed.
  ///
  /// @param[in] test_case The test case we want to add.
  ///
  /// @param[in] result The result of the test case.
  void AddTestResult(const TestCase& test_case, TestResult result) {
    TestRecordTrieNode* curr_node = root_;

    // Calculate path to the test, including directories, test file, test suite,
    // and test name
    std::filesystem::path path_to_test =
        std::filesystem::path(test_case.file_name) / test_case.suite_name /
        test_case.test_name;

    // Walk curr_node through the Trie to the test, creating new
    // TestRecordTrieNodes along the way if needed
    for (auto dir_entry : path_to_test) {
      if (auto search = curr_node->children.find(dir_entry.string());
          search != curr_node->children.end()) {
        curr_node = search->second;
      } else {
        TestRecordTrieNode* child_node = new TestRecordTrieNode();
        child_node->prefix = dir_entry.string();
        curr_node->children[dir_entry.string()] = child_node;
        curr_node = child_node;
      }
    }

    // Add the test case's result
    curr_node->is_leaf = true;
    curr_node->actual_test_result = result;
  }

  /// Adds the test result expectation for a particular test case. Usually, we
  /// expect all test results to be PASS. However, unique cases like a test case
  /// using the GTEST_SKIP macro will result in the expected result being a SKIP
  /// instead of a PASS.
  ///
  /// @param[in] test_case The test case we want to add the expected result for.
  ///
  /// @param[in] result The expected result we want to add for the test case.
  void AddTestResultExpectation(const TestCase& test_case,
                                TestResult expected_result) {
    TestRecordTrieNode* curr_node = root_;

    // Calculate path to the test, including directories, test file, test suite,
    // and test name
    std::filesystem::path path_to_test =
        std::filesystem::path(test_case.file_name) / test_case.suite_name /
        test_case.test_name;

    // Walk curr_node through the Trie to the test, creating new
    // TestRecordTrieNodes along the way if needed
    for (auto dir_entry : path_to_test) {
      if (auto search = curr_node->children.find(dir_entry.string());
          search != curr_node->children.end()) {
        curr_node = search->second;
      } else {
        TestRecordTrieNode* child_node = new TestRecordTrieNode();
        child_node->prefix = dir_entry.string();
        curr_node->children[dir_entry.string()] = child_node;
        curr_node = child_node;
      }
    }

    // Add the test case's expected result
    curr_node->expected_test_result = expected_result;
  }

  /// Outputs the test record trie as a json string.
  ///
  /// @param[in] summary Test summary that includes counts for each test result
  /// type.
  ///
  /// @param[in] seconds_since_epoch Seconds since epoch for when the test run
  /// started.
  ///
  /// @param[in] max_json_buffer_size The max size (in bytes) of the buffer to
  /// allocate for the json string.
  ///
  /// @param[in] interrupted Whether this test run was interrupted or not.
  ///
  /// @param[in] version Version of the test result JSON format.
  ///
  /// @returns The test record json as a string.
  std::string GetTestRecordJsonString(
      const RunTestsSummary& summary,
      int64_t seconds_since_epoch,
      size_t max_json_buffer_size,
      bool interrupted = false,
      int version = kJsonTestResultsFormatVersion) {
    // Dynamically allocate a string to serve as the json buffer.
    std::string buffer(max_json_buffer_size, '\0');
    JsonBuilder builder(buffer.data(), max_json_buffer_size);
    JsonObject& object = builder.StartObject();
    NestedJsonObject tests_json_object = object.AddNestedObject("tests");
    GetTestRecordJsonHelper(root_, tests_json_object);

    // Add test record metadata
    object.Add("version", version);
    object.Add("interrupted", interrupted);
    object.Add("seconds_since_epoch", seconds_since_epoch);
    NestedJsonObject num_failures_json =
        object.AddNestedObject("num_failures_by_type");
    num_failures_json.Add("PASS", summary.passed_tests);
    num_failures_json.Add("FAIL", summary.failed_tests);
    num_failures_json.Add("SKIP", summary.skipped_tests);

    // If the json buffer size was not big enough, then throw an error
    PW_CHECK(
        object.ok(),
        "Test record json buffer is not big enough, please increase size.");

    return object.data();
  }

 private:
  /// Used to represent a singular node of the trie.
  struct TestRecordTrieNode {
    /// Either the name of a directory, file, test suite, or test case.
    std::string prefix = "";

    /// Whether this node is a leaf in the trie. Leaf nodes represent the
    /// results of a singular test case and contains both the expected and
    /// actual test result of that test case.
    bool is_leaf = false;

    /// The expected test result for this node. Success is expected by default.
    TestResult expected_test_result = TestResult::kSuccess;

    /// The actual test result for this node. Empty if this is not a leaf node.
    TestResult actual_test_result{};

    /// Children of the current trie node, keyed by the child's prefix.
    std::unordered_map<std::string, TestRecordTrieNode*> children{};
  };

  /// Recursively convert the test record trie into a json object.
  ///
  /// @param[in] curr_node The current node we want to turn into the json
  /// object.
  ///
  /// @param[in] curr_json The json object to add new child json objects to.
  void GetTestRecordJsonHelper(TestRecordTrieNode* curr_node,
                               NestedJsonObject& curr_json) {
    if (curr_node->is_leaf) {
      NestedJsonObject child_json =
          curr_json.AddNestedObject(curr_node->prefix);
      child_json.Add("expected",
                     GetTestResultString(curr_node->expected_test_result));
      child_json.Add("actual",
                     GetTestResultString(curr_node->actual_test_result));
    } else {
      // Don't create a json object for the root TrieNode
      if (curr_node->prefix == "test_results") {
        for (const auto& child_entry : curr_node->children) {
          GetTestRecordJsonHelper(child_entry.second, curr_json);
        }
      } else {
        NestedJsonObject child_json =
            curr_json.AddNestedObject(curr_node->prefix);
        for (const auto& child_entry : curr_node->children) {
          GetTestRecordJsonHelper(child_entry.second, child_json);
        }
      }
    }
  }

  /// Helper to output a string representation of a `TestResult` object
  ///
  /// @param[in] test_result Test result to output as a string
  ///
  /// @returns A string representation of the passed in `TestResult` object
  std::string GetTestResultString(TestResult test_result) {
    switch (test_result) {
      case TestResult::kFailure:
        return "FAIL";
      case TestResult::kSuccess:
        return "PASS";
      case TestResult::kSkipped:
        return "SKIP";
    }
    return "UNKNOWN";
  }

  /// Helper to recursively delete a test record trie node and all its children
  ///
  /// @param[in] curr_node Node to delete, along with its children.
  void DeleteTestRecordTrie(TestRecordTrieNode* curr_node) {
    for (const auto& child : curr_node->children) {
      DeleteTestRecordTrie(child.second);
    }
    delete curr_node;
  }

  /// The root node of the test record trie
  TestRecordTrieNode* root_;
};

}  // namespace pw::unit_test::json_impl