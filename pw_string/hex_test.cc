// Copyright 2019 The Pigweed Authors
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

#include "pw_string/hex.h"

#include <cstdarg>

namespace pw::string {

static_assert(HexToNibble('0') == 0);
static_assert(HexToNibble('1') == 1);
static_assert(HexToNibble('2') == 2);
static_assert(HexToNibble('3') == 3);
static_assert(HexToNibble('4') == 4);
static_assert(HexToNibble('5') == 5);
static_assert(HexToNibble('6') == 6);
static_assert(HexToNibble('7') == 7);
static_assert(HexToNibble('8') == 8);
static_assert(HexToNibble('9') == 9);
static_assert(HexToNibble('A') == 0xA);
static_assert(HexToNibble('B') == 0xB);
static_assert(HexToNibble('C') == 0xC);
static_assert(HexToNibble('D') == 0xD);
static_assert(HexToNibble('E') == 0xE);
static_assert(HexToNibble('F') == 0xF);
static_assert(HexToNibble('a') == 0xA);
static_assert(HexToNibble('b') == 0xB);
static_assert(HexToNibble('c') == 0xC);
static_assert(HexToNibble('d') == 0xD);
static_assert(HexToNibble('e') == 0xE);
static_assert(HexToNibble('f') == 0xF);
static_assert(HexToNibble('@') == 0x100);

static_assert(NibbleToHex(0) == '0');
static_assert(NibbleToHex(1) == '1');
static_assert(NibbleToHex(2) == '2');
static_assert(NibbleToHex(3) == '3');
static_assert(NibbleToHex(4) == '4');
static_assert(NibbleToHex(5) == '5');
static_assert(NibbleToHex(6) == '6');
static_assert(NibbleToHex(7) == '7');
static_assert(NibbleToHex(8) == '8');
static_assert(NibbleToHex(9) == '9');
static_assert(NibbleToHex(0xA) == 'a');
static_assert(NibbleToHex(0xB) == 'b');
static_assert(NibbleToHex(0xC) == 'c');
static_assert(NibbleToHex(0xD) == 'd');
static_assert(NibbleToHex(0xE) == 'e');
static_assert(NibbleToHex(0xF) == 'f');
static_assert(NibbleToHex(0xFF) == '?');

static_assert(IsHexDigit('0'));
static_assert(IsHexDigit('1'));
static_assert(IsHexDigit('2'));
static_assert(IsHexDigit('3'));
static_assert(IsHexDigit('4'));
static_assert(IsHexDigit('5'));
static_assert(IsHexDigit('6'));
static_assert(IsHexDigit('7'));
static_assert(IsHexDigit('8'));
static_assert(IsHexDigit('9'));
static_assert(IsHexDigit('A'));
static_assert(IsHexDigit('B'));
static_assert(IsHexDigit('C'));
static_assert(IsHexDigit('D'));
static_assert(IsHexDigit('E'));
static_assert(IsHexDigit('F'));
static_assert(IsHexDigit('a'));
static_assert(IsHexDigit('b'));
static_assert(IsHexDigit('c'));
static_assert(IsHexDigit('d'));
static_assert(IsHexDigit('e'));
static_assert(IsHexDigit('f'));
static_assert(!IsHexDigit('!'));

}  // namespace pw::string
