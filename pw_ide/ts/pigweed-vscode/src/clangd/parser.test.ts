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

import { inferTarget, inferTargetPositions } from './parser';

test('inferTargetPositions', () => {
  const testCases: [string, number[]][] = [
    ['?', [0]],
    ['*/?', [1]],
    ['*/*/?', [2]],
    ['*/?/?', [1, 2]],
  ];

  for (const [glob, positions] of testCases) {
    expect(inferTargetPositions(glob)).toEqual(positions);
  }
});

test('inferTarget', () => {
  const testCases: [string, string, string][] = [
    ['?', 'target/thing.o', 'target'],
    ['*/?', 'variants/target/foo/bar/thing.o', 'target'],
    ['*/*/*/*/?', 'foo/bar/baz/hi/target/obj/thing.o', 'target'],
    ['*/?/?', 'variants/target/foo/bar/thing.o', 'target_foo'],
  ];

  for (const [glob, path, name] of testCases) {
    expect(inferTarget(glob, path)).toEqual(name);
  }
});
