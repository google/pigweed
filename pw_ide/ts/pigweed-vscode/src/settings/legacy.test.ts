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

import { loadLegacySettings } from './legacy';

describe('load legacy settings', () => {
  test('successfully loads valid settings', async () => {
    const settingsData = `config_title: pw_ide
default_target: pw_strict_host_clang_debug
compdb_gen_cmd: gn gen out`;

    const settings = await loadLegacySettings(settingsData);
    expect(settings).toHaveProperty('default_target');
    expect(settings?.default_target).toBe('pw_strict_host_clang_debug');
  });

  test('returns null on invalid settings', async () => {
    const settingsData = `default_target:
  - pw_strict_host_clang_debug`;

    const settings = await loadLegacySettings(settingsData);
    expect(settings).toBeNull();
  });

  test('returns null on unparsable settings', async () => {
    const settingsData = '- 2a05:4800:1:100::';

    const settings = await loadLegacySettings(settingsData);
    expect(settings).toBeNull();
  });
});
