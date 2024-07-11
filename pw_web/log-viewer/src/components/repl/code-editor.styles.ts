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

import { css } from 'lit';

export const styles = css`
  .editor-and-run {
    align-items: flex-end;
    display: flex;
    gap: 0.5rem;
    height: 15vh;
    padding: 1rem;
  }

  .editor {
    align-self: center;
    height: 100%;
    flex: 1;
  }

  .editor .cm-editor {
    background-color: var(--sys-repl-color-bg);
    border: var(--sys-repl-view-outline-width) solid
      var(--sys-repl-color-view-outline);
    border-radius: var(--sys-repl-view-corner-radius);
    height: 100%;
    overflow: hidden;
  }

  .editor .cm-gutters {
    background-color: var(--sys-repl-gutter-bg);
  }

  .editor .cm-activeLineGutter {
    background-color: var(--sys-repl-active-gutter-bg);
    color: var(--md-sys-color-on-primary-container);
  }
`;
