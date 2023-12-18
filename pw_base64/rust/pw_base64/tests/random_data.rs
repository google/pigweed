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

pub fn test_cases() -> Vec<(&'static [u8], &'static str)> {
    vec![
        (&b"\x74\x6d"[..], "dG0="),
        (&b"\x22\x86"[..], "IoY="),
        (&b"\xc0\xa2\x1c"[..], "wKIc"),
        (&b"\xa9\x67\xfb"[..], "qWf7"),
        (&b"\x77\xe1\x63\x51"[..], "d+FjUQ=="),
        (&b"\x7d\xa6\x8c\x5e"[..], "faaMXg=="),
        (&b"\x68\xaa\x19\x59\xd0"[..], "aKoZWdA="),
        (&b"\x46\x73\xd3\x54\x7e"[..], "RnPTVH4="),
        (&b"\x3f\xe8\x18\x4c\xe8\xf4"[..], "P+gYTOj0"),
        (&b"\x0a\xdd\x39\xbc\x1f\x65"[..], "Ct05vB9l"),
        (&b"\xc4\x5e\x4a\x6d\x4a\x04\xb6"[..], "xF5KbUoEtg=="),
        (&b"\x12\xe9\xf4\xaa\x2e\x4c\x31"[..], "Eun0qi5MMQ=="),
        (&b"\x55\x8c\x60\xcc\xc4\x7d\x99\x1f"[..], "VYxgzMR9mR8="),
        (&b"\xee\x21\x88\x2a\x0f\x7e\x76\xd7"[..], "7iGIKg9+dtc="),
        (&b"\xba\x40\x1d\x06\x92\xce\xc2\x8a\x28"[..], "ukAdBpLOwooo"),
        (&b"\xcc\x89\xf5\xeb\x49\x91\xa6\xa6\x88"[..], "zIn160mRpqaI"),
        (
            &b"\x55\x6b\x11\xe4\xc2\x22\xb0\x40\x14\x53"[..],
            "VWsR5MIisEAUUw==",
        ),
        (
            &b"\xd3\x1e\xc4\xe5\x06\x60\x37\x51\x10\x48"[..],
            "0x7E5QZgN1EQSA==",
        ),
        (
            &b"\x98\xae\x09\x8c\x61\x40\xbf\x77\xde\xd9\x0d"[..],
            "mK4JjGFAv3fe2Q0=",
        ),
        (
            &b"\x86\x39\x06\xa1\xc6\xfc\xcf\x30\x21\xba\xdf"[..],
            "hjkGocb8zzAhut8=",
        ),
    ]
}
