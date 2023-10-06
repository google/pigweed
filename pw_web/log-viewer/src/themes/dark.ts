// Copyright 2023 The Pigweed Authors
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

export const themeDark = css`
  /* Automatic theme styles */
  @media (prefers-color-scheme: dark) {
    :host {
      color-scheme: dark;

      /* Material Design tokens */
      --md-sys-color-primary: #a8c7fa;
      --md-sys-color-primary-60: #4c8df6;
      --md-sys-color-primary-container: #0842a0;
      --md-sys-color-on-primary: #062e6f;
      --md-sys-color-on-primary-container: #d3e3fd;
      --md-sys-color-inverse-primary: #0b57d0;
      --md-sys-color-secondary: #7fcfff;
      --md-sys-color-secondary-container: #004a77;
      --md-sys-color-on-secondary: #003355;
      --md-sys-color-on-secondary-container: #c2e7ff;
      --md-sys-color-tertiary: #6dd58c;
      --md-sys-color-tertiary-container: #0f5223;
      --md-sys-color-on-tertiary: #0a3818;
      --md-sys-color-on-tertiary-container: #c4eed0;
      --md-sys-color-surface: #131314;
      --md-sys-color-surface-dim: #131314;
      --md-sys-color-surface-bright: #37393b;
      --md-sys-color-surface-container-lowest: #0e0e0e;
      --md-sys-color-surface-container-low: #1b1b1b;
      --md-sys-color-surface-container: #1e1f20;
      --md-sys-color-surface-container-high: #282a2c;
      --md-sys-color-surface-container-highest: #333537;
      --md-sys-color-on-surface: #e3e3e3;
      --md-sys-color-on-surface-variant: #c4c7c5;
      --md-sys-color-inverse-surface: #e3e3e3;
      --md-sys-color-inverse-on-surface: #303030;
      --md-sys-color-outline: #8e918f;
      --md-sys-color-outline-variant: #444746;
      --md-sys-color-shadow: #000000;
      --md-sys-color-scrim: #000000;
      --md-sys-color-inverse-surface-rgb: 230, 225, 229;

      /* General */
      --sys-log-viewer-color-primary: var(--md-sys-color-primary);
      --sys-log-viewer-color-on-primary: var(--md-sys-color-on-primary);

      /* Log Viewer */
      --sys-log-viewer-color-bg: var(--md-sys-color-surface);

      /* Log View */
      --sys-log-viewer-color-view-outline: var(--md-sys-color-outline-variant);

      /* Log View Controls */
      --sys-log-viewer-color-controls-bg: var(
        --md-sys-color-surface-container-high
      );
      --sys-log-viewer-color-controls-text: var(
        --md-sys-color-on-surface-variant
      );
      --sys-log-viewer-color-controls-input-outline: transparent;
      --sys-log-viewer-color-controls-input-bg: var(--md-sys-color-surface);
      --sys-log-viewer-color-controls-button-enabled: var(
        --md-sys-color-primary-container
      );

      /* Log List */
      --sys-log-viewer-color-table-header-bg: var(
        --md-sys-color-surface-container
      );
      --sys-log-viewer-color-table-header-text: var(--md-sys-color-on-surface);
      --sys-log-viewer-color-table-bg: var(
        --md-sys-color-surface-container-lowest
      );
      --sys-log-viewer-color-table-text: var(--md-sys-color-on-surface);
      --sys-log-viewer-color-table-cell-outline: var(
        --md-sys-color-outline-variant
      );
      --sys-log-viewer-color-overflow-indicator: var(
        --md-sys-color-surface-container-lowest
      );
      --sys-log-viewer-color-table-mark: var(--md-sys-color-primary-container);
      --sys-log-viewer-color-table-mark-text: var(
        --md-sys-color-on-primary-container
      );
      --sys-log-viewer-color-table-mark-outline: var(
        --md-sys-color-outline-variant
      );
      --sys-log-viewer-color-table-row-highlight: var(
        --md-sys-color-inverse-surface-rgb
      );

      /* Severity */
      --sys-log-viewer-color-error-bright: #e46962;
      --sys-log-viewer-color-surface-error: #601410;
      --sys-log-viewer-color-on-surface-error: #f9dedc;
      --sys-log-viewer-color-orange-bright: #ee9836;
      --sys-log-viewer-color-surface-yellow: #402d00;
      --sys-log-viewer-color-on-surface-yellow: #ffdfa0;
      --sys-log-viewer-color-debug: var(--md-sys-color-primary-60);
    }
  }

  /* Manual theme styles */
  :host([colorscheme='dark']) {
    color-scheme: dark;

    /* Material Design tokens */
    --md-sys-color-primary: #a8c7fa;
    --md-sys-color-primary-60: #4c8df6;
    --md-sys-color-primary-container: #0842a0;
    --md-sys-color-on-primary: #062e6f;
    --md-sys-color-on-primary-container: #d3e3fd;
    --md-sys-color-inverse-primary: #0b57d0;
    --md-sys-color-secondary: #7fcfff;
    --md-sys-color-secondary-container: #004a77;
    --md-sys-color-on-secondary: #003355;
    --md-sys-color-on-secondary-container: #c2e7ff;
    --md-sys-color-tertiary: #6dd58c;
    --md-sys-color-tertiary-container: #0f5223;
    --md-sys-color-on-tertiary: #0a3818;
    --md-sys-color-on-tertiary-container: #c4eed0;
    --md-sys-color-surface: #131314;
    --md-sys-color-surface-dim: #131314;
    --md-sys-color-surface-bright: #37393b;
    --md-sys-color-surface-container-lowest: #0e0e0e;
    --md-sys-color-surface-container-low: #1b1b1b;
    --md-sys-color-surface-container: #1e1f20;
    --md-sys-color-surface-container-high: #282a2c;
    --md-sys-color-surface-container-highest: #333537;
    --md-sys-color-on-surface: #e3e3e3;
    --md-sys-color-on-surface-variant: #c4c7c5;
    --md-sys-color-inverse-surface: #e3e3e3;
    --md-sys-color-inverse-on-surface: #303030;
    --md-sys-color-outline: #8e918f;
    --md-sys-color-outline-variant: #444746;
    --md-sys-color-shadow: #000000;
    --md-sys-color-scrim: #000000;
    --md-sys-color-inverse-surface-rgb: 230, 225, 229;

    /* General */
    --sys-log-viewer-color-primary: var(--md-sys-color-primary);
    --sys-log-viewer-color-on-primary: var(--md-sys-color-on-primary);

    /* Log Viewer */
    --sys-log-viewer-color-bg: var(--md-sys-color-surface);

    /* Log View */
    --sys-log-viewer-color-view-outline: var(--md-sys-color-outline-variant);

    /* Log View Controls */
    --sys-log-viewer-color-controls-bg: var(
      --md-sys-color-surface-container-high
    );
    --sys-log-viewer-color-controls-text: var(
      --md-sys-color-on-surface-variant
    );
    --sys-log-viewer-color-controls-input-outline: transparent;
    --sys-log-viewer-color-controls-input-bg: var(--md-sys-color-surface);
    --sys-log-viewer-color-controls-button-enabled: var(
      --md-sys-color-primary-container
    );

    /* Log List */
    --sys-log-viewer-color-table-header-bg: var(
      --md-sys-color-surface-container
    );
    --sys-log-viewer-color-table-header-text: var(--md-sys-color-on-surface);
    --sys-log-viewer-color-table-bg: var(
      --md-sys-color-surface-container-lowest
    );
    --sys-log-viewer-color-table-text: var(--md-sys-color-on-surface);
    --sys-log-viewer-color-table-cell-outline: var(
      --md-sys-color-outline-variant
    );
    --sys-log-viewer-color-overflow-indicator: var(
      --md-sys-color-surface-container-lowest
    );
    --sys-log-viewer-color-table-mark: var(--md-sys-color-primary-container);
    --sys-log-viewer-color-table-mark-text: var(
      --md-sys-color-on-primary-container
    );
    --sys-log-viewer-color-table-mark-outline: var(
      --md-sys-color-outline-variant
    );
    --sys-log-viewer-color-table-row-highlight: var(
      --md-sys-color-inverse-surface-rgb
    );

    /* Severity */
    --sys-log-viewer-color-error-bright: #e46962;
    --sys-log-viewer-color-surface-error: #601410;
    --sys-log-viewer-color-on-surface-error: #f9dedc;
    --sys-log-viewer-color-orange-bright: #ee9836;
    --sys-log-viewer-color-surface-yellow: #402d00;
    --sys-log-viewer-color-on-surface-yellow: #ffdfa0;
    --sys-log-viewer-color-debug: var(--md-sys-color-primary-60);
  }
`;
