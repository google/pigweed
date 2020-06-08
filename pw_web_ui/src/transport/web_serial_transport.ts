// Copyright 2020 The Pigweed Authors
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

/* eslint-env browser */
import {BehaviorSubject, Subject} from 'rxjs';
import DeviceTransport from './device_transport';

const DEFAULT_SERIAL_OPTIONS: SerialOptions = {
  baudrate: 921600,
  databits: 8,
  parity: 'none',
  stopbits: 1,
};

/**
 * WebSerialTransport sends and receives UInt8Arrays to and
 * from a serial device connected over USB.
 */
export class WebSerialTransport implements DeviceTransport {
  chunks = new Subject<Uint8Array>();
  connected = new BehaviorSubject<boolean>(false);
  private writer?: WritableStreamDefaultWriter<Uint8Array>;

  constructor(
    private serial: Serial = navigator.serial,
    private filters: SerialPortFilter[] = [],
    private serialOptions = DEFAULT_SERIAL_OPTIONS
  ) {}

  /**
   * Send a UInt8Array chunk of data to the connected device.
   * @param {Uint8Array} chunk The chunk to send
   */
  async sendChunk(chunk: Uint8Array): Promise<void> {
    if (this.writer !== undefined && this.connected.getValue()) {
      await this.writer.ready;
      return this.writer.write(chunk);
    }
    throw new Error('Device not connected');
  }

  /**
   * Attempt to open a connection to a device. This includes
   * asking the user to select a serial port and should only
   * be called in response to user interaction.
   */
  async connect(): Promise<void> {
    let port: SerialPort;
    try {
      port = await this.serial.requestPort({filters: this.filters});
    } catch (e) {
      // Ignore errors where the user did not select a port.
      if (!(e instanceof DOMException)) {
        throw e;
      }
      return;
    }

    await port.open(this.serialOptions);
    this.writer = port.writable.getWriter();

    this.getChunks(port);
  }

  private getChunks(port: SerialPort) {
    port.readable.pipeTo(
      new WritableStream({
        write: chunk => {
          this.chunks.next(chunk);
        },
        close: () => {
          port.close();
          this.writer?.releaseLock();
          this.connected.next(false);
        },
        abort: () => {
          // Reconnect to the port
          this.connected.next(false);
          this.getChunks(port);
        },
      })
    );
    this.connected.next(true);
  }
}
