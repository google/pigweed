// Copyright 2021 The Pigweed Authors
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

/** Low-level HDLC protocol features. */
import {Buffer} from 'buffer';
import {crc32} from 'crc';

/** Special flag character for delimiting HDLC frames. */
export const FLAG = 0x7e;

/** Special character for escaping other special characters in a frame. */
export const ESCAPE = 0x7d;

/** Frame control for unnumbered information */
export const UI_FRAME_CONTROL = frameControl(0x00);

/** Calculates the CRC32 of |data| */
export function frameCheckSequence(data: Uint8Array): Uint8Array {
  const crc = crc32(Buffer.from(data.buffer, data.byteOffset, data.byteLength));
  const arr = new ArrayBuffer(4);
  const view = new DataView(arr);
  view.setUint32(0, crc, true);  // litteEndian = true
  return new Uint8Array(arr);
}

/** Encodes an HDLC address as a one-terminated LSB varint. */
export function encodeAddress(address: number): Uint8Array {
  const byteList = [];
  while (true) {
    byteList.push((address & 0x7f) << 1);
    address >>= 7;
    if (address === 0) {
      break;
    }
  }

  let result = Uint8Array.from(byteList);
  result[result.length - 1] |= 0x1;
  return result;
}

function frameControl(frameType: number): Uint8Array {
  return Uint8Array.from([0x03 | frameType]);
}
