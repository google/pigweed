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
#include "pw_cpu_exception_armv7m/cpu_state.h"
#include "pw_cpu_exception_armv7m_protos/cpu_state.pwpb.h"
#include "pw_preprocessor/compiler.h"
#include "pw_protobuf/encoder.h"

namespace pw::cpu_exception {

Status DumpCpuStateProto(protobuf::Encoder& dest,
                         const pw_CpuExceptionState& cpu_state) {
  armv7m::ArmV7mCpuState::Encoder state_encoder(&dest);

  // Special and mem-mapped registers.
  state_encoder.WritePc(cpu_state.base.pc);
  state_encoder.WriteLr(cpu_state.base.lr);
  state_encoder.WritePsr(cpu_state.base.psr);
  state_encoder.WriteMsp(cpu_state.extended.msp);
  state_encoder.WritePsp(cpu_state.extended.psp);
  state_encoder.WriteExcReturn(cpu_state.extended.exc_return);
  state_encoder.WriteCfsr(cpu_state.extended.cfsr);
  state_encoder.WriteMmfar(cpu_state.extended.mmfar);
  state_encoder.WriteBfar(cpu_state.extended.bfar);
  state_encoder.WriteIcsr(cpu_state.extended.icsr);
  state_encoder.WriteHfsr(cpu_state.extended.hfsr);
  state_encoder.WriteShcsr(cpu_state.extended.shcsr);
  state_encoder.WriteControl(cpu_state.extended.control);

  // General purpose registers.
  state_encoder.WriteR0(cpu_state.base.r0);
  state_encoder.WriteR1(cpu_state.base.r1);
  state_encoder.WriteR2(cpu_state.base.r2);
  state_encoder.WriteR3(cpu_state.base.r3);
  state_encoder.WriteR4(cpu_state.extended.r4);
  state_encoder.WriteR5(cpu_state.extended.r5);
  state_encoder.WriteR6(cpu_state.extended.r6);
  state_encoder.WriteR7(cpu_state.extended.r7);
  state_encoder.WriteR8(cpu_state.extended.r8);
  state_encoder.WriteR9(cpu_state.extended.r9);
  state_encoder.WriteR10(cpu_state.extended.r10);
  state_encoder.WriteR11(cpu_state.extended.r11);

  // If the encode buffer was exhausted in an earlier write, it will be
  // reflected here.
  Status status = state_encoder.WriteR12(cpu_state.base.r12);
  if (!status.ok()) {
    return status == Status::ResourceExhausted() ? Status::ResourceExhausted()
                                                 : Status::Unknown();
  }
  return Status::Ok();
}

}  // namespace pw::cpu_exception
