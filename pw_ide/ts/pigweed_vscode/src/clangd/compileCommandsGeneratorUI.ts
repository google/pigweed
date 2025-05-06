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

import { Logger } from '../loggingTypes';

export class UIManager {
  private stdoutBuffer: string[] = [];
  private stderrBuffer: string[] = [];
  private statusLine = '';
  private isTuiActive = false;
  private partialStdoutLine = '';
  private partialStderrLine = '';

  private static readonly ESC = '\x1B';
  private static readonly CSI = UIManager.ESC + '[';
  private static readonly CLEAR_LINE = `${UIManager.CSI}2K`;
  private static readonly HIDE_CURSOR = `${UIManager.CSI}?25l`;
  private static readonly SHOW_CURSOR = `${UIManager.CSI}?25h`;

  private write(str: string): void {
    process.stdout.write(str);
  }

  private clearCurrentTui(): void {
    if (!process.stdout.isTTY) return;

    // Move cursor to the beginning of the current line and clear it.
    this.write('\r' + UIManager.CLEAR_LINE);
  }

  private render(): void {
    if (!process.stdout.isTTY) return;
    this.clearCurrentTui();
    this.write(this.statusLine);

    this.isTuiActive = true;
  }

  // Helper to process buffered data into complete lines and store them.
  private processData(
    data: string | Buffer,
    partialLineProp: 'partialStdoutLine' | 'partialStderrLine',
    buffer: string[],
  ): void {
    const currentData = this[partialLineProp] + data.toString();
    const lines = currentData.split(/\r?\n/);

    // Keep the last potentially incomplete line in the partial line property.
    this[partialLineProp] = lines.pop() || '';

    // Process complete lines
    lines.forEach((line) => buffer.push(line));
  }

  public updateStatus(status: string): void {
    this.statusLine = status;
    if (this.isTuiActive) {
      this.render();
    } else if (process.stdout.isTTY) {
      // First time rendering for a TTY: hide cursor, then render.
      this.write(UIManager.HIDE_CURSOR);
      this.render();
    }
    // If not TTY, status updates do nothing visually.
  }

  public addStdout(data: string | Buffer): void {
    this.processData(data, 'partialStdoutLine', this.stdoutBuffer);
  }

  public addStderr(data: string | Buffer): void {
    this.processData(data, 'partialStderrLine', this.stderrBuffer);
  }

  public finish(finalStatus: string): void {
    const canUseTui = process.stdout.isTTY;

    if (canUseTui && this.isTuiActive) {
      this.clearCurrentTui();
    }
    process.stdout.write(finalStatus + '\n');

    if (canUseTui) {
      this.write(UIManager.SHOW_CURSOR);
    }

    this.isTuiActive = false;
  }

  public finishWithError(finalStatus: string): void {
    const canUseTui = process.stdout.isTTY;

    if (canUseTui && this.isTuiActive) {
      // 1. Clear the existing TUI area (status + stdout window).
      //    clearCurrentTui handles moving the cursor up, clearing lines,
      //    and positioning the cursor back at the start of the (now blank) status line.
      this.clearCurrentTui();

      // 2. Write the final status line in the cleared top line space.
      //    Ensure the line is clear before writing.
      this.write(UIManager.CLEAR_LINE + finalStatus + '\n');
    } else {
      // If not a TTY or TUI was never active (nothing rendered),
      // just print the final status plainly.
      process.stdout.write(finalStatus + '\n');
    }

    // Print buffered stderr
    if (this.stderrBuffer.length > 0 || this.partialStderrLine) {
      this.stderrBuffer.forEach((line) => process.stdout.write(line + '\n'));
      // Add partial line if any
      if (this.partialStderrLine) {
        process.stdout.write(this.partialStderrLine + '\n');
      }
    }

    // 3. Ensure cursor is visible (only if TTY)
    if (canUseTui) {
      this.write(UIManager.SHOW_CURSOR);
    }

    // 4. Mark TUI as definitively inactive
    this.isTuiActive = false;
  }

  public cleanup(): void {
    if (!process.stdout.isTTY) return;
    // Explicitly show cursor in case of unexpected exit
    if (this.isTuiActive) this.write(UIManager.SHOW_CURSOR);
  }
}

export class LoggerUI {
  private stdoutBuffer: string[] = [];
  private stderrBuffer: string[] = [];
  private partialStdoutLine = '';
  private partialStderrLine = '';

  constructor(private logger: Logger) {}

  private processData(
    data: string | Buffer,
    partialLineProp: 'partialStdoutLine' | 'partialStderrLine',
    buffer: string[],
  ): void {
    const currentData = this[partialLineProp] + data.toString();
    const lines = currentData.split(/\r?\n/);
    this[partialLineProp] = lines.pop() || '';
    lines.forEach((line) => buffer.push(line));
  }

  public updateStatus(status: string): void {
    this.logger.info(status);
  }

  public addStdout(data: string | Buffer): void {
    this.processData(data, 'partialStdoutLine', this.stdoutBuffer);
  }

  public addStderr(data: string | Buffer): void {
    this.processData(data, 'partialStderrLine', this.stderrBuffer);
  }

  public finish(finalStatus: string): void {
    this.logger.info(finalStatus);
  }

  public finishWithError(finalStatus: string): void {
    this.logger.error(finalStatus);

    if (this.stderrBuffer.length > 0 || this.partialStderrLine) {
      this.stderrBuffer.forEach((line) => this.logger.error(line));
      if (this.partialStderrLine) {
        this.logger.error(this.partialStderrLine);
      }
    }
  }

  public cleanup(): void {
    // no-op
  }
}
