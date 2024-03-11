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

package dev.pigweed.pw_rpc;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

public final class ServiceTest {
  private static final Service SERVICE = new Service("pw.rpc.test1.TheTestService",
      Service.unaryMethod("SomeUnary", SomeMessage.parser(), AnotherMessage.parser()),
      Service.serverStreamingMethod(
          "SomeServerStreaming", SomeMessage.parser(), AnotherMessage.parser()),
      Service.clientStreamingMethod(
          "SomeClientStreaming", SomeMessage.parser(), AnotherMessage.parser()),
      Service.bidirectionalStreamingMethod(
          "SomeBidiStreaming", SomeMessage.parser(), AnotherMessage.parser()));

  private static final Method METHOD_1 = SERVICE.method("SomeUnary");
  private static final Method METHOD_2 = SERVICE.method("SomeServerStreaming");
  private static final Method METHOD_3 = SERVICE.method("SomeClientStreaming");
  private static final Method METHOD_4 = SERVICE.method("SomeBidiStreaming");

  @Test
  public void getMethods_includesAllMethods() {
    assertThat(SERVICE.getMethods()).containsExactly(METHOD_1, METHOD_2, METHOD_3, METHOD_4);
  }
}
