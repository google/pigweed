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
    --sys-repl-color-bg: var(--md-sys-color-surface-container-high);

    color: var(--md-sys-color-on-surface);
    display: flex;
    font-family: 'Roboto Mono', monospace;
    flex-direction: column;
    overflow: hidden;
    height: 100%;
    width: 100%;
    contain: style;
  }

  #repl {
    display: flex;
    flex-direction: column;
    height: 100%;
    gap: 1rem;
  }

  #repl #output {
    margin: 0;
    padding: 0.5rem 1rem;
    flex: 1;
    background-color: var(--sys-repl-color-bg);
    border: var(--sys-repl-view-outline-width) solid
      var(--sys-repl-color-view-outline);
    border-radius: var(--sys-repl-view-corner-radius);
  }
  #repl #output li {
    list-style: none;
    border-left: 3px solid transparent;
    padding-left: 1rem;
    margin: 5px 0rem;
    display: flex;
    flex-direction: column;
  }
  #repl #output li:last-child {
    border-left: 3px solid #1976d2;
  }
  #repl #output .stderr {
    color: #d32f2f;
  }
`;
