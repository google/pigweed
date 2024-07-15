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

import * as vscode from 'vscode';

const bugUrl =
  'https://issues.pigweed.dev/issues/new?component=1194524&template=1911548';

/**
 * Open the bug report template in the user's browser.
 */
export function fileBug() {
  vscode.env.openExternal(vscode.Uri.parse(bugUrl));
}
