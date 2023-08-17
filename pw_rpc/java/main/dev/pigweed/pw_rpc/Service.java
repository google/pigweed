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

package dev.pigweed.pw_rpc;

import com.google.common.collect.ImmutableCollection;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;
import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;
import java.util.stream.Collectors;

/** Represents an RPC service: a collection of related methods. */
public class Service {
  private final String name;
  private final int id;
  private final ImmutableMap<Integer, Method> methods;

  public Service(String name, Method.Builder... methodBuilders) {
    this.name = name;
    this.id = Ids.calculate(name);
    Arrays.stream(methodBuilders).forEach(m -> m.setService(this));
    this.methods = ImmutableMap.copyOf(Arrays.stream(methodBuilders)
                                           .map(Method.Builder::build)
                                           .collect(Collectors.toMap(Method::id, m -> m)));
  }

  /** Returns the fully qualified name of this service (package.Service). */
  public final String name() {
    return name;
  }

  /** Returns the methods in this service. */
  public final ImmutableCollection<Method> getMethods() {
    return methods.values();
  }

  final int id() {
    return id;
  }

  final Method method(String name) {
    return methods.get(Ids.calculate(name));
  }

  final Method method(int id) {
    return methods.get(id);
  }

  @Override
  public final String toString() {
    return name();
  }

  // TODO(b/293361955): Remove deprecated methods.

  /**
   * Declares a unary service method.
   *
   * @param name The method name within the service, e.g. "MyMethod" for my_pkg.MyService.MyMethod.
   * @param request Parser for the request protobuf, e.g. MyRequestProto.parser()
   * @param response Parser for the response protobuf, e.g. MyResponseProto.parser()
   * @return Method.Builder, for internal use by the Service class only
   */
  public static Method.Builder unaryMethod(
      String name, Parser<? extends MessageLite> request, Parser<? extends MessageLite> response) {
    return Method.builder()
        .setType(Method.Type.UNARY)
        .setName(name)
        .setRequestParser(request)
        .setResponseParser(response);
  }

  /**
   * Declares a unary service method.
   *
   * @deprecated Pass `ProtobufType.parser()` instead of `ProtobufType.class`.
   */
  @Deprecated
  public static Method.Builder unaryMethod(
      String name, Class<? extends MessageLite> request, Class<? extends MessageLite> response) {
    return unaryMethod(name, getParser(request), getParser(response));
  }

  /**
   * Declares a server streaming service method.
   *
   * @param name The method name within the service, e.g. "MyMethod" for my_pkg.MyService.MyMethod.
   * @param request Parser for the request protobuf, e.g. MyRequestProto.parser()
   * @param response Parser for the response protobuf, e.g. MyResponseProto.parser()
   * @return Method.Builder, for internal use by the Service class only
   */
  public static Method.Builder serverStreamingMethod(
      String name, Parser<? extends MessageLite> request, Parser<? extends MessageLite> response) {
    return Method.builder()
        .setType(Method.Type.SERVER_STREAMING)
        .setName(name)
        .setRequestParser(request)
        .setResponseParser(response);
  }

  /**
   * Declares a server streaming service method.
   *
   * @deprecated Pass `ProtobufType.parser()` instead of `ProtobufType.class`.
   */
  @Deprecated
  public static Method.Builder serverStreamingMethod(
      String name, Class<? extends MessageLite> request, Class<? extends MessageLite> response) {
    return serverStreamingMethod(name, getParser(request), getParser(response));
  }

  /**
   * Declares a client streaming service method.
   *
   * @param name The method name within the service, e.g. "MyMethod" for my_pkg.MyService.MyMethod.
   * @param request Parser for the request protobuf, e.g. MyRequestProto.parser()
   * @param response Parser for the response protobuf, e.g. MyResponseProto.parser()
   * @return Method.Builder, for internal use by the Service class only
   */
  public static Method.Builder clientStreamingMethod(
      String name, Parser<? extends MessageLite> request, Parser<? extends MessageLite> response) {
    return Method.builder()
        .setType(Method.Type.CLIENT_STREAMING)
        .setName(name)
        .setRequestParser(request)
        .setResponseParser(response);
  }

  /**
   * Declares a client streaming service method.
   *
   * @deprecated Pass `ProtobufType.parser()` instead of `ProtobufType.class`.
   */
  @Deprecated
  public static Method.Builder clientStreamingMethod(
      String name, Class<? extends MessageLite> request, Class<? extends MessageLite> response) {
    return clientStreamingMethod(name, getParser(request), getParser(response));
  }

  /**
   * Declares a bidirectional streaming service method.
   *
   * @param name The method name within the service, e.g. "MyMethod" for my_pkg.MyService.MyMethod.
   * @param request Parser for the request protobuf, e.g. MyRequestProto.parser()
   * @param response Parser for the response protobuf, e.g. MyResponseProto.parser()
   * @return Method.Builder, for internal use by the Service class only
   */
  public static Method.Builder bidirectionalStreamingMethod(
      String name, Parser<? extends MessageLite> request, Parser<? extends MessageLite> response) {
    return Method.builder()
        .setType(Method.Type.BIDIRECTIONAL_STREAMING)
        .setName(name)
        .setRequestParser(request)
        .setResponseParser(response);
  }

  /**
   * Declares a bidirectional streaming service method.
   *
   * @deprecated Pass `ProtobufType.parser()` instead of `ProtobufType.class`.
   */
  @Deprecated
  public static Method.Builder bidirectionalStreamingMethod(
      String name, Class<? extends MessageLite> request, Class<? extends MessageLite> response) {
    return bidirectionalStreamingMethod(name, getParser(request), getParser(response));
  }

  /**
   * Gets the Parser from a protobuf class using reflection.
   *
   * This function is provided for backwards compatibility with the deprecated service API that
   * takes a class object instead of a protobuf parser object.
   */
  @SuppressWarnings("unchecked")
  private static Parser<? extends MessageLite> getParser(Class<? extends MessageLite> messageType) {
    try {
      return (Parser<? extends MessageLite>) messageType.getMethod("parser").invoke(null);
    } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
      throw new LinkageError(
          String.format("Service method was created with %s, which does not have parser() method; "
                  + "either the class is not a generated protobuf class "
                  + "or the parser() method was optimized out (see b/293361955)",
              messageType),
          e);
    }
  }
}
