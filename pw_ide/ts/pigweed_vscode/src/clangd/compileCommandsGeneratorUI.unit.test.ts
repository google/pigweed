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

import * as assert from 'assert';
import { UIManager } from './compileCommandsGeneratorUI';

// ANSI constants
const ESC = '\x1B';
const CSI = ESC + '[';
const CLEAR_LINE = `${CSI}2K`;
const HIDE_CURSOR = `${CSI}?25l`;
const SHOW_CURSOR = `${CSI}?25h`;

let mockStdout: any;
const originalStdout = process.stdout;
let writtenOutput: string;

function setupTest(isTTY = true, columns = 80): void {
  writtenOutput = '';
  mockStdout = {
    write: (str: string) => {
      writtenOutput += str;
    },
    isTTY: isTTY,
    columns: columns,
  };
  Object.defineProperty(process, 'stdout', {
    value: mockStdout,
    writable: true,
    configurable: true,
  });
}

function teardownTest(): void {
  Object.defineProperty(process, 'stdout', {
    value: originalStdout,
    writable: true,
    configurable: true,
  });
}

test('CompilerCommandGeneratorUI: constructor creates instance', () => {
  setupTest();
  try {
    const ui = new UIManager();
    assert.ok(ui instanceof UIManager, 'Should create a UIManager instance');
  } finally {
    teardownTest();
  }
});

// --- updateStatus Tests ---
test('CompilerCommandGeneratorUI: updateStatus first call (TTY) initializes TUI', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Starting...');

    const expectedOutput = HIDE_CURSOR + '\r' + CLEAR_LINE + 'Starting...';
    assert.strictEqual(writtenOutput, expectedOutput);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, true, 'TUI should be active');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: updateStatus subsequent call (TTY) updates status line', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Status 1');
    writtenOutput = '';

    ui.updateStatus('Status 2');
    const expectedOutput = '\r' + CLEAR_LINE + 'Status 2';
    assert.strictEqual(writtenOutput, expectedOutput);
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: updateStatus does nothing visually if not TTY', () => {
  setupTest(false);
  try {
    const ui = new UIManager();
    ui.updateStatus('Status 1');
    assert.strictEqual(writtenOutput, '', 'Should write nothing if not TTY');
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, false, 'TUI should not be active');
    ui.updateStatus('Status 2');
    assert.strictEqual(writtenOutput, '', 'Should still write nothing');
  } finally {
    teardownTest();
  }
});

// --- addStdout / addStderr Tests ---
test('CompilerCommandGeneratorUI: addStdout buffers data, no visual output (TTY)', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Initial Status'); // Activate TUI
    writtenOutput = ''; // Clear initial render

    ui.addStdout('Stdout line 1\n');
    ui.addStdout('Partial stdout');

    assert.strictEqual(
      writtenOutput,
      '',
      'addStdout should produce no visual output',
    );
    assert.deepStrictEqual(
      // @ts-expect-error Accessing private member
      ui.stdoutBuffer,
      ['Stdout line 1'],
      'Should buffer complete lines',
    );
    assert.strictEqual(
      // @ts-expect-error Accessing private member
      ui.partialStdoutLine,
      'Partial stdout',
      'Should store partial line',
    );
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: addStderr buffers data, no visual output (TTY)', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Initial Status');
    writtenOutput = '';

    ui.addStderr('Stderr line 1\n');
    ui.addStderr('Partial stderr');

    assert.strictEqual(
      writtenOutput,
      '',
      'addStderr should produce no visual output',
    );
    assert.deepStrictEqual(
      // @ts-expect-error Accessing private member
      ui.stderrBuffer,
      ['Stderr line 1'],
      'Should buffer complete lines',
    );
    assert.strictEqual(
      // @ts-expect-error Accessing private member
      ui.partialStderrLine,
      'Partial stderr',
      'Should store partial line',
    );
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: addStdout/addStderr buffer data (Non-TTY)', () => {
  setupTest(false);
  try {
    const ui = new UIManager();
    ui.addStdout('Stdout line 1\n');
    ui.addStderr('Stderr line 1\n');
    ui.addStdout('Partial stdout');
    ui.addStderr('Partial stderr');

    assert.strictEqual(writtenOutput, '', 'Should produce no visual output');
    // @ts-expect-error Accessing private member
    assert.deepStrictEqual(ui.stdoutBuffer, ['Stdout line 1']);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.partialStdoutLine, 'Partial stdout');
    // @ts-expect-error Accessing private member
    assert.deepStrictEqual(ui.stderrBuffer, ['Stderr line 1']);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.partialStderrLine, 'Partial stderr');
  } finally {
    teardownTest();
  }
});

// --- finish Tests ---
test('CompilerCommandGeneratorUI: finish clears TUI and shows final status (TTY, Active)', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Working...');
    writtenOutput = '';

    ui.finish('All Done.');

    // Expected: Clear line, print final status + newline, show cursor
    const expectedOutput = '\r' + CLEAR_LINE + 'All Done.\n' + SHOW_CURSOR;
    assert.strictEqual(writtenOutput, expectedOutput);
    assert.strictEqual(
      // @ts-expect-error Accessing private member
      ui.isTuiActive,
      false,
      'TUI should be inactive after finish',
    );
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: finish only prints status if TUI never active (TTY)', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    // No updateStatus call, TUI is inactive
    writtenOutput = '';

    ui.finish('Simple Finish.');

    // Expected: Print final status + newline, show cursor (no clear)
    const expectedOutput = 'Simple Finish.\n' + SHOW_CURSOR;
    assert.strictEqual(writtenOutput, expectedOutput);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, false, 'TUI should remain inactive');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: finish prints plain status if not TTY', () => {
  setupTest(false);
  try {
    const ui = new UIManager();
    ui.updateStatus('Status');
    ui.addStdout('Output\n');
    ui.addStderr('Error\n');
    writtenOutput = '';

    ui.finish('Final Non-TTY Status.');

    // Expected: Print final status + newline (no ANSI codes)
    assert.strictEqual(writtenOutput, 'Final Non-TTY Status.\n');
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, false, 'TUI should remain inactive');
  } finally {
    teardownTest();
  }
});

// --- finishWithError Tests ---
test('CompilerCommandGeneratorUI: finishWithError clears TUI, shows status, dumps stderr (TTY, Active)', () => {
  setupTest(true); // isTTY = true
  try {
    const ui = new UIManager();
    ui.updateStatus('Working...');
    ui.addStdout('Stdout line 1\n');
    ui.addStderr('Stderr line 1\n');
    ui.addStdout('Partial stdout');
    ui.addStderr('Partial stderr');
    writtenOutput = '';

    ui.finishWithError('Error Occurred.');

    // Corrected Expected: Clear TUI status, print final status (with its own clear), print ONLY stderr buffers (no delimiters), show cursor
    const expectedOutput =
      '\r' +
      CLEAR_LINE + // Clear TUI status line before printing final status
      CLEAR_LINE +
      'Error Occurred.\n' + // Print final status (with own clear for safety)
      'Stderr line 1\n' + // Print stderr buffer
      'Partial stderr\n' + // Print partial stderr
      SHOW_CURSOR; // Show cursor

    assert.strictEqual(writtenOutput, expectedOutput);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, false, 'TUI should be inactive');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: finishWithError shows status, dumps stderr (TTY, Inactive)', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    // TUI is never activated
    ui.addStdout('Stdout line 1\n');
    ui.addStderr('Stderr line 1\n');
    ui.addStdout('Partial stdout');
    ui.addStderr('Partial stderr');
    writtenOutput = '';

    ui.finishWithError('Error Occurred.');

    // Corrected Expected: Print final status, print ONLY stderr buffers (no delimiters), show cursor (no TUI clear)
    const expectedOutput =
      'Error Occurred.\n' + // Print final status
      'Stderr line 1\n' + // Print stderr buffer
      'Partial stderr\n' + // Print partial stderr
      SHOW_CURSOR; // Show cursor

    assert.strictEqual(writtenOutput, expectedOutput);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, false, 'TUI should remain inactive');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: finishWithError shows status, dumps stderr (Non-TTY)', () => {
  setupTest(false);
  try {
    const ui = new UIManager();
    ui.addStdout('Stdout line 1\n');
    ui.addStderr('Stderr line 1\n');
    ui.addStdout('Partial stdout');
    ui.addStderr('Partial stderr');
    writtenOutput = '';

    ui.finishWithError('Error Occurred.');

    // Corrected Expected: Print final status, print ONLY stderr buffers (no delimiters, no ANSI codes)
    const expectedOutput =
      'Error Occurred.\n' + // Print final status
      'Stderr line 1\n' + // Print stderr buffer
      'Partial stderr\n'; // Print partial stderr

    assert.strictEqual(writtenOutput, expectedOutput);
    // @ts-expect-error Accessing private member
    assert.strictEqual(ui.isTuiActive, false, 'TUI should remain inactive');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: finishWithError handles empty stderr buffers', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Working...');
    ui.addStdout('Some output\n');
    writtenOutput = '';

    ui.finishWithError('Completed with Issues.');

    // Expected: Clear line, print final status + newline, show cursor (no stderr buffer output)
    const expectedOutput =
      '\r' + CLEAR_LINE + CLEAR_LINE + 'Completed with Issues.\n' + SHOW_CURSOR;

    assert.strictEqual(writtenOutput, expectedOutput);
  } finally {
    teardownTest();
  }
});

// --- cleanup Tests ---
test('CompilerCommandGeneratorUI: cleanup shows cursor if TTY and TUI was active', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    ui.updateStatus('Activate');
    writtenOutput = '';
    ui.cleanup();
    assert.strictEqual(writtenOutput, SHOW_CURSOR, 'Should show cursor');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: cleanup does nothing if TTY but TUI was not active', () => {
  setupTest(true);
  try {
    const ui = new UIManager();
    writtenOutput = '';
    ui.cleanup();
    assert.strictEqual(writtenOutput, '', 'Should do nothing');
  } finally {
    teardownTest();
  }
});

test('CompilerCommandGeneratorUI: cleanup does nothing if not TTY', () => {
  setupTest(false); // isTTY = false
  try {
    const ui = new UIManager();
    ui.updateStatus('Activate');
    writtenOutput = '';
    ui.cleanup();
    assert.strictEqual(writtenOutput, '', 'Should do nothing');
  } finally {
    teardownTest();
  }
});
