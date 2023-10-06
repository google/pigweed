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

export const themeLight = css`
  /* Automatic theme styles */
  @media (prefers-color-scheme: light) {
    :host {
      color-scheme: light;

      /* Material Design tokens */
      --md-sys-color-primary: #0b57d0;
      --md-sys-color-primary-70: #7cacf8;
      --md-sys-color-primary-90: #d3e3fd;
      --md-sys-color-primary-95: #ecf3fe;
      --md-sys-color-primary-99: #fafbff;
      --md-sys-color-primary-container: #d3e3fd;
      --md-sys-color-on-primary: #ffffff;
      --md-sys-color-on-primary-container: #041e49;
      --md-sys-color-inverse-primary: #a8c7fa;
      --md-sys-color-secondary: #00639b;
      --md-sys-color-secondary-container: #c2e7ff;
      --md-sys-color-on-secondary: #ffffff;
      --md-sys-color-on-secondary-container: #001d35;
      --md-sys-color-tertiary: #146c2e;
      --md-sys-color-tertiary-container: #c4eed0;
      --md-sys-color-on-tertiary: #ffffff;
      --md-sys-color-on-tertiary-container: #072711;
      --md-sys-color-surface: #ffffff;
      --md-sys-color-surface-dim: #d3dbe5;
      --md-sys-color-surface-bright: #ffffff;
      --md-sys-color-surface-container-lowest: #ffffff;
      --md-sys-color-surface-container-low: #f8fafd;
      --md-sys-color-surface-container: #f0f4f9;
      --md-sys-color-surface-container-high: #e9eef6;
      --md-sys-color-surface-container-highest: #dde3ea;
      --md-sys-color-on-surface: #1f1f1f;
      --md-sys-color-on-surface-variant: #444746;
      --md-sys-color-inverse-surface: #303030;
      --md-sys-color-inverse-on-surface: #f2f2f2;
      --md-sys-color-outline: #747775;
      --md-sys-color-outline-variant: #c4c7c5;
      --md-sys-color-shadow: #000000;
      --md-sys-color-scrim: #000000;
      --md-sys-color-inverse-surface-rgb: 49, 48, 51;

      /* General */
      --sys-log-viewer-color-primary: var(--md-sys-color-primary);
      --sys-log-viewer-color-on-primary: var(--md-sys-color-on-primary);

      /* Log Viewer */
      --sys-log-viewer-color-bg: var(--md-sys-color-surface);

      /* Log View */
      --sys-log-viewer-color-view-outline: var(--md-sys-color-outline);

      /* Log View Controls */
      --sys-log-viewer-color-controls-bg: var(--md-sys-color-primary-90);
      --sys-log-viewer-color-controls-text: var(
        --md-sys-color-on-primary-container
      );
      --sys-log-viewer-color-controls-input-outline: transparent;
      --sys-log-viewer-color-controls-input-bg: var(
        --md-sys-color-surface-container-lowest
      );
      --sys-log-viewer-color-controls-button-enabled: var(
        --md-sys-color-primary-70
      );

      /* Log List */
      --sys-log-viewer-color-table-header-bg: var(--md-sys-color-primary-95);
      --sys-log-viewer-color-table-header-text: var(--md-sys-color-on-surface);
      --sys-log-viewer-color-table-bg: var(
        --md-sys-color-surface-container-lowest
      );
      --sys-log-viewer-color-table-text: var(--md-sys-color-on-surface);
      --sys-log-viewer-color-table-cell-outline: var(
        --md-sys-color-outline-variant
      );
      --sys-log-viewer-color-overflow-indicator: var(
        --md-sys-color-surface-container
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
      --sys-log-viewer-color-error-bright: #dc362e;
      --sys-log-viewer-color-surface-error: #fcefee;
      --sys-log-viewer-color-on-surface-error: #8c1d18;
      --sys-log-viewer-color-orange-bright: #f49f2a;
      --sys-log-viewer-color-surface-yellow: #fef9eb;
      --sys-log-viewer-color-on-surface-yellow: #783616;
      --sys-log-viewer-color-debug: var(--md-sys-color-primary);
    }
  }

  /* Manual theme styles */
  :host([colorscheme='light']) {
    color-scheme: light;

    /* Material Design tokens */
    --md-sys-color-primary: #0b57d0;
    --md-sys-color-primary-70: #7cacf8;
    --md-sys-color-primary-90: #d3e3fd;
    --md-sys-color-primary-95: #ecf3fe;
    --md-sys-color-primary-99: #fafbff;
    --md-sys-color-primary-container: #d3e3fd;
    --md-sys-color-on-primary: #ffffff;
    --md-sys-color-on-primary-container: #041e49;
    --md-sys-color-inverse-primary: #a8c7fa;
    --md-sys-color-secondary: #00639b;
    --md-sys-color-secondary-container: #c2e7ff;
    --md-sys-color-on-secondary: #ffffff;
    --md-sys-color-on-secondary-container: #001d35;
    --md-sys-color-tertiary: #146c2e;
    --md-sys-color-tertiary-container: #c4eed0;
    --md-sys-color-on-tertiary: #ffffff;
    --md-sys-color-on-tertiary-container: #072711;
    --md-sys-color-surface: #ffffff;
    --md-sys-color-surface-dim: #d3dbe5;
    --md-sys-color-surface-bright: #ffffff;
    --md-sys-color-surface-container-lowest: #ffffff;
    --md-sys-color-surface-container-low: #f8fafd;
    --md-sys-color-surface-container: #f0f4f9;
    --md-sys-color-surface-container-high: #e9eef6;
    --md-sys-color-surface-container-highest: #dde3ea;
    --md-sys-color-on-surface: #1f1f1f;
    --md-sys-color-on-surface-variant: #444746;
    --md-sys-color-inverse-surface: #303030;
    --md-sys-color-inverse-on-surface: #f2f2f2;
    --md-sys-color-outline: #747775;
    --md-sys-color-outline-variant: #c4c7c5;
    --md-sys-color-shadow: #000000;
    --md-sys-color-scrim: #000000;
    --md-sys-color-inverse-surface-rgb: 49, 48, 51;

    /* General */
    --sys-log-viewer-color-primary: var(--md-sys-color-primary);
    --sys-log-viewer-color-on-primary: var(--md-sys-color-on-primary);

    /* Log Viewer */
    --sys-log-viewer-color-bg: var(--md-sys-color-surface);

    /* Log View */
    --sys-log-viewer-color-view-outline: var(--md-sys-color-outline);

    /* Log View Controls */
    --sys-log-viewer-color-controls-bg: var(--md-sys-color-primary-90);
    --sys-log-viewer-color-controls-text: var(
      --md-sys-color-on-primary-container
    );
    --sys-log-viewer-color-controls-input-outline: transparent;
    --sys-log-viewer-color-controls-input-bg: var(
      --md-sys-color-surface-container-lowest
    );
    --sys-log-viewer-color-controls-button-enabled: var(
      --md-sys-color-primary-70
    );

    /* Log List */
    --sys-log-viewer-color-table-header-bg: var(--md-sys-color-primary-95);
    --sys-log-viewer-color-table-header-text: var(--md-sys-color-on-surface);
    --sys-log-viewer-color-table-bg: var(
      --md-sys-color-surface-container-lowest
    );
    --sys-log-viewer-color-table-text: var(--md-sys-color-on-surface);
    --sys-log-viewer-color-table-cell-outline: var(
      --md-sys-color-outline-variant
    );
    --sys-log-viewer-color-overflow-indicator: var(
      --md-sys-color-surface-container
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
    --sys-log-viewer-color-error-bright: #dc362e;
    --sys-log-viewer-color-surface-error: #fcefee;
    --sys-log-viewer-color-on-surface-error: #8c1d18;
    --sys-log-viewer-color-orange-bright: #f49f2a;
    --sys-log-viewer-color-surface-yellow: #fef9eb;
    --sys-log-viewer-color-on-surface-yellow: #783616;
    --sys-log-viewer-color-debug: var(--md-sys-color-primary);
  }
`;
