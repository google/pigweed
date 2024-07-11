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
  * {
    box-sizing: border-box;
  }

  :host {
    /* Repl */
    --sys-repl-height: 100%;
    --sys-repl-view-outline-width: 1px;
    --sys-repl-view-corner-radius: 0.5rem;

    --sys-repl-table-cell-padding: 0.375rem 0.75rem;
    --sys-repl-table-cell-icon-size: 1.125rem;

    --sys-repl-header-bg: var(--sys-repl-gutter-bg);
    --sys-repl-color-bg: var(--md-sys-color-surface-container);
    --sys-repl-color-outline: var(--md-sys-color-outline-variant);
    --sys-repl-color-error-bright: var(--sys-log-viewer-color-error-bright);

    color: var(--md-sys-color-on-surface);
    display: flex;
    font-family: 'Roboto Mono', monospace;
    flex-direction: column;
    height: 100%;
    width: 100%;
  }

  .header {
    align-items: center;
    background-color: var(--sys-repl-header-bg);
    border-bottom: 1px solid var(--sys-repl-color-outline);
    color: var(--sys-log-viewer-color-controls-text);
    display: flex;
    padding: 0 1rem;
    font-size: 1.125rem;
    font-weight: 300;
    min-height: 3rem;
  }

  #repl {
    border: var(--sys-repl-view-outline-width) solid
      var(--sys-repl-color-outline);
    border-radius: var(--sys-repl-view-corner-radius);
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
  }

  #repl .stdin {
    color: var(--md-sys-color-primary);
    font-weight: bold;
  }

  #repl #output {
    border-radius: var(--sys-repl-view-corner-radius);
    flex: 1;
    margin: 0;
    padding: 0.5rem 1rem;
    overflow-y: auto;
  }

  #repl #output li {
    border-left: 3px solid transparent;
    display: flex;
    flex-direction: column;
    list-style: none;
    margin: 5px 0rem;
    padding-left: 1rem;
  }

  #repl #output li:last-child {
    border-left: 3px solid var(--md-sys-color-primary);
  }

  #repl #output .stderr {
    color: var(--sys-repl-color-error-bright);
  }
`;
