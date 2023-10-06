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

export const styles = css`
  * {
    box-sizing: border-box;
  }

  :host {
    display: flex;
    flex-direction: column;
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
  }

  .grid-container {
    display: grid;
    grid-gap: 1rem;
    grid-template-columns: repeat(auto-fit, minmax(27rem, 1fr));
    height: 100%;
    overflow: hidden;
  }

  .add-button {
    width: 8rem;
    height: 2rem;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
  }
`;
