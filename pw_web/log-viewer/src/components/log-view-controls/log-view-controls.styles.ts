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
  :host {
    align-items: center;
    background-color: var(--sys-log-viewer-color-controls-bg);
    color: var(--sys-log-viewer-color-controls-text);
    display: flex;
    flex-shrink: 0;
    gap: 1rem;
    height: 3rem;
    justify-content: space-between;
    padding: 0 1rem;
  }

  :host > * {
    display: flex;
  }

  p {
    white-space: nowrap;
  }

  .field-menu {
    background-color: var(--md-sys-color-surface-container);
    border-radius: 4px;
    margin: 0;
    padding: 0.5rem 0.75rem;
    position: absolute;
    right: 0;
    z-index: 2;
  }

  .button-toggle-enabled {
    background-color: var(--sys-log-viewer-color-controls-button-enabled);
    border-radius: 100%;
  }

  .field-menu-item {
    align-items: center;
    display: flex;
    height: 3rem;
    width: max-content;
  }

  .field-toggle {
    border-radius: 1.5rem;
    position: relative;
  }

  .input-container {
    width: 100%;
  }

  input {
    background-color: var(--sys-log-viewer-color-controls-input-bg);
    border: none;
    border-radius: 1.5rem;
    font-family: 'Google Sans';
    height: 1.75rem;
    max-width: 20rem;
    padding: 0 1rem;
    width: 100%;
    border: 1px solid var(--sys-log-viewer-color-controls-input-outline);
  }

  input::placeholder {
    color: var(--md-sys-color-on-surface-variant);
  }

  input[type='checkbox'] {
    accent-color: var(--md-sys-color-primary);
    height: 1.125rem;
    width: 1.125rem;
  }

  label {
    padding-left: 0.75rem;
  }

  p {
    flex: 1 0;
    white-space: nowrap;
  }
`;
