# Copyright 2020 The Pigweed Authors
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
"""Cortex-M architecture related constants."""

import collections

# Cortex-M (ARMv7-M + ARMv8-M) related constants.
# These values are from the ARMv7-M Architecture Reference Manual DDI 0403E.b
# and ARMv8-M Architecture Reference Manual DDI 0553A.e.
# https =//static.docs.arm.com/ddi0403/e/DDI0403E_B_armv7m_arm.pdf
# https =//static.docs.arm.com/ddi0553/a/DDI0553A_e_armv8m_arm.pdf

# Exception ISR number. (ARMv7-M Section B1.5.2)
# When the ISR number (accessible from ICSR and PSR) is zero, it indicates the
# core is in thread mode.
PW_CORTEX_M_THREAD_MODE_ISR_NUM = 0x0
PW_CORTEX_M_NMI_ISR_NUM = 0x2
PW_CORTEX_M_HARD_FAULT_ISR_NUM = 0x3
PW_CORTEX_M_MEM_FAULT_ISR_NUM = 0x4
PW_CORTEX_M_BUS_FAULT_ISR_NUM = 0x5
PW_CORTEX_M_USAGE_FAULT_ISR_NUM = 0x6

# Masks for Interrupt Control and State Register ICSR (ARMv7-M Section B3.2.4)
PW_CORTEX_M_ICSR_VECTACTIVE_MASK = (1 << 9) - 1

# Masks for individual bits of HFSR. (ARMv7-M Section B3.2.16)
PW_CORTEX_M_HFSR_FORCED_MASK = 0x1 << 30

# Masks for different sections of CFSR. (ARMv7-M Section B3.2.15)
PW_CORTEX_M_CFSR_MEM_FAULT_MASK = 0x000000ff
PW_CORTEX_M_CFSR_BUS_FAULT_MASK = 0x0000ff00
PW_CORTEX_M_CFSR_USAGE_FAULT_MASK = 0xffff0000

# Masks for individual bits of CFSR. (ARMv7-M Section B3.2.15)
# Memory faults (MemManage Status Register) =
PW_CORTEX_M_CFSR_MEM_FAULT_START = (0x1)
PW_CORTEX_M_CFSR_IACCVIOL_MASK = (PW_CORTEX_M_CFSR_MEM_FAULT_START << 0)
PW_CORTEX_M_CFSR_DACCVIOL_MASK = (PW_CORTEX_M_CFSR_MEM_FAULT_START << 1)
PW_CORTEX_M_CFSR_MUNSTKERR_MASK = (PW_CORTEX_M_CFSR_MEM_FAULT_START << 3)
PW_CORTEX_M_CFSR_MSTKERR_MASK = (PW_CORTEX_M_CFSR_MEM_FAULT_START << 4)
PW_CORTEX_M_CFSR_MLSPERR_MASK = (PW_CORTEX_M_CFSR_MEM_FAULT_START << 5)
PW_CORTEX_M_CFSR_MMARVALID_MASK = (PW_CORTEX_M_CFSR_MEM_FAULT_START << 7)
# Bus faults (BusFault Status Register) =
PW_CORTEX_M_CFSR_BUS_FAULT_START = (0x1 << 8)
PW_CORTEX_M_CFSR_IBUSERR_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 0)
PW_CORTEX_M_CFSR_PRECISERR_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 1)
PW_CORTEX_M_CFSR_IMPRECISERR_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 2)
PW_CORTEX_M_CFSR_UNSTKERR_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 3)
PW_CORTEX_M_CFSR_STKERR_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 4)
PW_CORTEX_M_CFSR_LSPERR_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 5)
PW_CORTEX_M_CFSR_BFARVALID_MASK = (PW_CORTEX_M_CFSR_BUS_FAULT_START << 7)
# Usage faults (UsageFault Status Register) =
PW_CORTEX_M_CFSR_USAGE_FAULT_START = (0x1 << 16)
PW_CORTEX_M_CFSR_UNDEFINSTR_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 0)
PW_CORTEX_M_CFSR_INVSTATE_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 1)
PW_CORTEX_M_CFSR_INVPC_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 2)
PW_CORTEX_M_CFSR_NOCP_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 3)
PW_CORTEX_M_CFSR_STKOF_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 4)
PW_CORTEX_M_CFSR_UNALIGNED_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 8)
PW_CORTEX_M_CFSR_DIVBYZERO_MASK = (PW_CORTEX_M_CFSR_USAGE_FAULT_START << 9)

# TODO(amontanez): We could probably make a whole module on bit field handling
# in python.
BitField = collections.namedtuple('BitField',
                                  ['name', 'bit_mask', 'description'])

PW_CORTEX_M_CFSR_BIT_FIELDS = [
    BitField('IACCVIOL', PW_CORTEX_M_CFSR_IACCVIOL_MASK,
             'MPU violation on instruction fetch.'),
    BitField('DACCVIOL', PW_CORTEX_M_CFSR_DACCVIOL_MASK,
             'MPU violation on memory read/write.'),
    BitField('MUNSTKERR', PW_CORTEX_M_CFSR_MUNSTKERR_MASK,
             'MPU violation on exception return.'),
    BitField('MSTKERR', PW_CORTEX_M_CFSR_MSTKERR_MASK,
             'MPU violation on exception entry.'),
    BitField('MLSPERR', PW_CORTEX_M_CFSR_MLSPERR_MASK,
             'FPU lazy state preservation failed.'),
    BitField('MMARVALID', PW_CORTEX_M_CFSR_MMARVALID_MASK,
             'MMFAR register is valid.'),
    BitField('IBUSERR', PW_CORTEX_M_CFSR_IBUSERR_MASK,
             'Bus fault on instruction fetch.'),
    BitField('PRECISERR', PW_CORTEX_M_CFSR_PRECISERR_MASK,
             'Precise bus fault.'),
    BitField('IMPRECISERR', PW_CORTEX_M_CFSR_IMPRECISERR_MASK,
             'Imprecise bus fault.'),
    BitField('UNSTKERR', PW_CORTEX_M_CFSR_UNSTKERR_MASK,
             'Hardware failure on context restore.'),
    BitField('STKERR', PW_CORTEX_M_CFSR_STKERR_MASK,
             'Hardware failure on context save.'),
    BitField('LSPERR', PW_CORTEX_M_CFSR_LSPERR_MASK,
             'FPU lazy state preservation failed.'),
    BitField('BFARVALID', PW_CORTEX_M_CFSR_BFARVALID_MASK, 'BFAR is valid.'),
    BitField('UNDEFINSTR', PW_CORTEX_M_CFSR_UNDEFINSTR_MASK,
             'Encountered invalid instruction.'),
    BitField('INVSTATE', PW_CORTEX_M_CFSR_INVSTATE_MASK,
             ('Attempted to execute an instruction with an invalid Execution '
              'Program Status Register (EPSR) value.')),
    BitField('INVPC', PW_CORTEX_M_CFSR_INVPC_MASK,
             'Program Counter (PC) is not legal.'),
    BitField('NOCP', PW_CORTEX_M_CFSR_NOCP_MASK,
             'Coprocessor disabled or not present.'),
    BitField('STKOF', PW_CORTEX_M_CFSR_STKOF_MASK, 'Stack overflowed.'),
    BitField('UNALIGNED', PW_CORTEX_M_CFSR_UNALIGNED_MASK,
             'Unaligned load or store. (This exception can be disabled)'),
    BitField('DIVBYZERO', PW_CORTEX_M_CFSR_DIVBYZERO_MASK, 'Divide by zero.'),
]
