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

import com.google.auto.value.AutoValue;
import com.google.common.base.Ascii;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;
import java.lang.reflect.InvocationTargetException;

/**
 * Information about an RPC service method.
 *
 * <p>If protobuf descriptors are available, this information can be extracted from the generated
 * protobuf descriptors. The "lite" Java protobuf library does not provide descriptors, so RPC
 * services and methods must be manually declared and provided to an RPC client.
 */
@AutoValue
public abstract class Method {
  public abstract Service service();

  public abstract String name();

  public abstract Type type();

  abstract Parser<? extends MessageLite> requestParser();

  abstract Parser<? extends MessageLite> responseParser();

  public final Class<? extends MessageLite> request() {
    return getProtobufClass(requestParser());
  }

  public final Class<? extends MessageLite> response() {
    return getProtobufClass(responseParser());
  }

  final int id() {
    return Ids.calculate(name());
  }

  public final boolean isServerStreaming() {
    return type().isServerStreaming();
  }

  public final boolean isClientStreaming() {
    return type().isClientStreaming();
  }

  public final String fullName() {
    return createFullName(service().name(), name());
  }

  static String createFullName(String serviceName, String methodName) {
    return serviceName + '/' + methodName;
  }

  static Builder builder() {
    return new AutoValue_Method.Builder();
  }

  @Override
  public final String toString() {
    return fullName();
  }

  /** Builds Method instances. */
  @AutoValue.Builder
  public abstract static class Builder {
    abstract Builder setService(Service value);

    abstract Builder setType(Type type);

    abstract Builder setName(String value);

    abstract Builder setRequestParser(Parser<? extends MessageLite> parser);

    abstract Builder setResponseParser(Parser<? extends MessageLite> parser);

    abstract Method build();
  }

  /** Decodes a response payload according to the method's response type. */
  final MessageLite decodeResponsePayload(ByteString data) throws InvalidProtocolBufferException {
    return responseParser().parseFrom(data);
  }

  private static Class<? extends MessageLite> getProtobufClass(
      Parser<? extends MessageLite> parser) {
    try {
      return parser.parseFrom(ByteString.EMPTY).getClass();
    } catch (InvalidProtocolBufferException e) {
      throw new AssertionError("Failed to parse zero bytes as a protobuf! "
              + "It was assumed that zero bytes is always a valid protobuf.",
          e);
    }
  }

  /** Which type of RPC this is: unary or server/client/bidirectional streaming. */
  public enum Type {
    UNARY(/* isServerStreaming= */ false, /* isClientStreaming= */ false),
    SERVER_STREAMING(/* isServerStreaming= */ true, /* isClientStreaming= */ false),
    CLIENT_STREAMING(/* isServerStreaming= */ false, /* isClientStreaming= */ true),
    BIDIRECTIONAL_STREAMING(/* isServerStreaming= */ true, /* isClientStreaming= */ true);

    private final boolean isServerStreaming;
    private final boolean isClientStreaming;

    Type(boolean isServerStreaming, boolean isClientStreaming) {
      this.isServerStreaming = isServerStreaming;
      this.isClientStreaming = isClientStreaming;
    }

    public final boolean isServerStreaming() {
      return isServerStreaming;
    }

    public final boolean isClientStreaming() {
      return isClientStreaming;
    }

    public final String sentenceName() {
      return Ascii.toLowerCase(name()).replace('_', ' ');
    }
  }
}
