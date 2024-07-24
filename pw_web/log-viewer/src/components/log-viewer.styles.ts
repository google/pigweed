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
    display: block;
    gap: 2rem;
    height: var(--sys-log-viewer-height);
    width: 100%;

    /* Material Web properties */
    --md-icon-font: 'Material Symbols Rounded';
    --md-icon-size: 1.25rem;
    --md-filled-button-label-text-type: 'Roboto Flex', Arial, sans-serif;
    --md-outlined-button-label-text-type: 'Roboto Flex', Arial, sans-serif;
    --md-icon-button-unselected-icon-color: var(
      --md-sys-color-on-surface-variant
    );
    --md-icon-button-unselected-hover-icon-color: var(
      --md-sys-color-on-primary-container
    );
    --md-filled-button-container-color: var(--sys-log-viewer-color-primary);

    /* Log View */
    --sys-log-viewer-height: 100%;
    --sys-log-viewer-view-outline-width: 1px;
    --sys-log-viewer-view-corner-radius: 0.5rem;

    /* Log List */
    --sys-log-viewer-table-cell-padding: 0.375rem 0.75rem;
    --sys-log-viewer-table-cell-icon-size: 1rem;

    /* Log View Controls */
    --sys-log-viewer-header-height: 2.75rem;
    --sys-log-viewer-header-title-font-size: 1rem;
  }

  sl-split-panel {
    --divider-width: 8px;
    --divider-hit-area: 24px;
    --min: 10rem;
    --max: calc(100% - 10rem);
    height: 100%;
    width: 100%;
    contain: size style;
  }

  sl-split-panel::part(divider) {
    border-radius: 8px;
    transition: 150ms ease 20ms;
    background-color: transparent;
    border: 2px solid black;
    border-color: var(--md-sys-color-surface);
  }

  sl-split-panel::part(divider):hover,
  sl-split-panel::part(divider):focus {
    background-color: #a8c7fa;
  }

  sl-split-panel div[slot='start'],
  sl-split-panel div[slot='end'] {
    overflow: hidden;
  }
`;
