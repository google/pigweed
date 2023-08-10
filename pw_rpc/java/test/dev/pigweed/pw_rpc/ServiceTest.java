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

  @Test
  public void serviceDeclaration_deprecatedClassBasedEquivalentToParserBased() {
    final Service declaredWithClassObjects = new Service(SERVICE.name(),
        Service.unaryMethod("SomeUnary", SomeMessage.class, AnotherMessage.class),
        Service.serverStreamingMethod(
            "SomeServerStreaming", SomeMessage.class, AnotherMessage.class),
        Service.clientStreamingMethod(
            "SomeClientStreaming", SomeMessage.class, AnotherMessage.class),
        Service.bidirectionalStreamingMethod(
            "SomeBidiStreaming", SomeMessage.class, AnotherMessage.class));

    assertThat(declaredWithClassObjects.method("SomeUnary").responseParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeUnary").responseParser());
    assertThat(declaredWithClassObjects.method("SomeServerStreaming").responseParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeServerStreaming").responseParser());
    assertThat(declaredWithClassObjects.method("SomeClientStreaming").responseParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeClientStreaming").responseParser());
    assertThat(declaredWithClassObjects.method("SomeBidiStreaming").responseParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeBidiStreaming").responseParser());

    assertThat(declaredWithClassObjects.method("SomeUnary").requestParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeUnary").requestParser());
    assertThat(declaredWithClassObjects.method("SomeServerStreaming").requestParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeServerStreaming").requestParser());
    assertThat(declaredWithClassObjects.method("SomeClientStreaming").requestParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeClientStreaming").requestParser());
    assertThat(declaredWithClassObjects.method("SomeBidiStreaming").requestParser())
        .isSameInstanceAs(declaredWithClassObjects.method("SomeBidiStreaming").requestParser());
  }
}
