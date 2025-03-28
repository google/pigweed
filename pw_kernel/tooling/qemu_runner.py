# Copyright 2025 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Utility to invoke qemu and pipe the output through the detokenizer.
"""

import argparse
import subprocess
import sys
import logging

from pw_tokenizer import detokenize

_LOG = logging.getLogger(__name__)
_LOG.setLevel(logging.INFO)

try:
    import qemu.qemu_system_arm
    import qemu.qemu_system_riscv32
    from python.runfiles import runfiles  # type: ignore

    r = runfiles.Create()
    _QEMU_ARM = r.Rlocation(
        *qemu.qemu_system_arm.RLOCATION,
    )
    _QEMU_RISCV32 = r.Rlocation(*qemu.qemu_system_riscv32.RLOCATION)
except ImportError:
    _LOG.fatal("runfiles could not find qemu")


# Dict mapping the cpu type to the correct qemu binary
_QEMU_BINARY_CPU_TYPE = dict[str, str](
    {
        'cortex-m0': _QEMU_ARM,
        'cortex-m3': _QEMU_ARM,
        'cortex-m33': _QEMU_ARM,
        'cortex-m4': _QEMU_ARM,
        'cortex-m55': _QEMU_ARM,
        'rv32': _QEMU_RISCV32,
        'rv32i': _QEMU_RISCV32,
        'rv32e': _QEMU_RISCV32,
    }
)


def _parse_args():
    """Parse and return command line arguments."""

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        '--machine',
        type=str,
        help='qemu machine type',
    )
    parser.add_argument(
        '--cpu',
        type=str,
        help='qemu cpu type',
    )
    parser.add_argument(
        '--image',
        type=str,
        help='image file to run',
    )
    parser.add_argument(
        '--semihosting',
        action='store_true',
        help='image file to run',
    )
    return parser.parse_args()


def _main(args) -> int:
    try:
        qemu_exe: str = _QEMU_BINARY_CPU_TYPE[args.cpu.lower()]
    except KeyError:
        _LOG.fatal("unknown cpu type: %s", args.cpu)

    qemu_args = [
        qemu_exe,
        "-machine",
        args.machine,
        "-cpu",
        args.cpu,
        "-bios",
        "none",
        "-nographic",
        "-serial",
        "mon:stdio",
        "-kernel",
        args.image,
    ]

    if args.semihosting:
        qemu_args += [
            "-semihosting-config",
            "enable=on,target=native",
        ]

    _LOG.info("Invoking QEMU: %s", qemu_args)
    process = subprocess.Popen(
        args=qemu_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    detokenizer = detokenize.Detokenizer(args.image)

    while True:
        output = process.stdout.readline()
        if output == '' and process.poll() is not None:
            break
        if output:
            print(detokenizer.detokenize_text(output.strip()))

    sys.exit(process.poll())


if __name__ == '__main__':
    _main(_parse_args())
