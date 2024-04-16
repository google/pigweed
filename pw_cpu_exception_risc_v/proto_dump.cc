// Copyright 2024 The Pigweed Authors
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
#include "pw_cpu_exception_risc_v/cpu_state.h"
#include "pw_cpu_exception_risc_v_protos/cpu_state.pwpb.h"
#include "pw_protobuf/encoder.h"

namespace pw::cpu_exception::risc_v {

Status DumpCpuStateProto(protobuf::StreamEncoder& dest,
                         const pw_cpu_exception_State& cpu_state) {
  // Note that the encoder's status is checked at the end and ignored at the
  // Write*() calls to reduce the number of branches.
  risc_v::RiscvCpuState::StreamEncoder& state_encoder =
      *static_cast<risc_v::RiscvCpuState::StreamEncoder*>(&dest);

  const ExtraRegisters& extended = cpu_state.extended;
  const ExceptionRegisters& base = cpu_state.base;

  state_encoder.WriteMepc(extended.mepc).IgnoreError();
  state_encoder.WriteMcause(extended.mcause).IgnoreError();
  state_encoder.WriteMtval(extended.mtval).IgnoreError();
  state_encoder.WriteMstatus(extended.mstatus).IgnoreError();

  // General purpose registers.
  state_encoder.WriteRa(base.ra).IgnoreError();
  state_encoder.WriteSp(base.sp).IgnoreError();
  state_encoder.WriteT0(base.t0).IgnoreError();
  state_encoder.WriteT1(base.t1).IgnoreError();
  state_encoder.WriteT2(base.t2).IgnoreError();
  state_encoder.WriteFp(base.fp).IgnoreError();
  state_encoder.WriteS1(base.s1).IgnoreError();
  state_encoder.WriteA0(base.a0).IgnoreError();
  state_encoder.WriteA1(base.a1).IgnoreError();
  state_encoder.WriteA2(base.a2).IgnoreError();
  state_encoder.WriteA3(base.a3).IgnoreError();
  state_encoder.WriteA4(base.a4).IgnoreError();
  state_encoder.WriteA5(base.a5).IgnoreError();
  state_encoder.WriteA6(base.a6).IgnoreError();
  state_encoder.WriteA7(base.a7).IgnoreError();
  state_encoder.WriteS2(base.s2).IgnoreError();
  state_encoder.WriteS3(base.s3).IgnoreError();
  state_encoder.WriteS4(base.s4).IgnoreError();
  state_encoder.WriteS5(base.s5).IgnoreError();
  state_encoder.WriteS6(base.s6).IgnoreError();
  state_encoder.WriteS7(base.s7).IgnoreError();
  state_encoder.WriteS8(base.s8).IgnoreError();
  state_encoder.WriteS9(base.s9).IgnoreError();
  state_encoder.WriteS10(base.s10).IgnoreError();
  state_encoder.WriteS11(base.s11).IgnoreError();
  state_encoder.WriteT3(base.t3).IgnoreError();
  state_encoder.WriteT4(base.t4).IgnoreError();
  state_encoder.WriteT5(base.t5).IgnoreError();
  state_encoder.WriteT6(base.t6).IgnoreError();

  // If the encode buffer was exhausted in an earlier write, it will be
  // reflected here.
  if (const Status status = state_encoder.status(); !status.ok()) {
    return status.IsResourceExhausted() ? Status::ResourceExhausted()
                                        : Status::Unknown();
  }

  return OkStatus();
}

}  // namespace pw::cpu_exception::risc_v
