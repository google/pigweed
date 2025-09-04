// Copyright 2025 The Pigweed Authors
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

/**
 * This is a Java Native Interface (JNI) wrapper for the Detokenizer class.
 */
package dev.pigweed.pw_tokenizer;

import com.github.fmeum.rules_jni.RulesJni;
import java.nio.charset.StandardCharsets;
import javax.annotation.Nullable;

/** This class provides the Java interface for the C++ Detokenizer class. */
public final class Detokenizer {
  static {
    RulesJni.loadLibrary("pw_tokenizer_jni", Detokenizer.class);
  }

  // The handle (pointer) to the C++ detokenizer instance.
  private final long handle;

  public static Detokenizer fromCsv(String csvTokenDatabase) {
    return new Detokenizer(
        newNativeDetokenizerCsv(csvTokenDatabase.getBytes(StandardCharsets.UTF_8)));
  }

  public static Detokenizer fromCsv(byte[] csvTokenDatabaseUtf8) {
    return new Detokenizer(newNativeDetokenizerCsv(csvTokenDatabaseUtf8));
  }

  public static Detokenizer fromBinary(byte[] binaryTokenDatabase) {
    return new Detokenizer(newNativeDetokenizerBinary(binaryTokenDatabase));
  }

  private Detokenizer(long nativeHandle) {
    handle = nativeHandle;
  }

  /**
   * Detokenizes the binary tokenized message without recursion.
   *
   * @return the detokenized string if there was one successful detokenization, otherwise null
   */
  @Nullable
  public String detokenize(byte[] binaryMessage) {
    byte[] bytes = detokenizeNative(handle, binaryMessage);
    return bytes != null ? new String(bytes, StandardCharsets.UTF_8) : null;
  }

  /**
   * Detokenizes and replaces all recognized nested tokenized messages ($) in the provided string.
   * Unrecognized tokenized strings are left unchanged.
   */
  public String detokenizeText(String message) {
    return new String(detokenizeTextNative(handle, message), StandardCharsets.UTF_8);
  }

  /** Deletes memory allocated in C++ when this class is garbage collected. */
  @Override
  protected void finalize() {
    deleteNativeDetokenizer(handle);
  }

  /** Creates a new detokenizer using the provided binary token database. */
  private static native long newNativeDetokenizerBinary(byte[] data);

  /** Creates a new detokenizer using the provided CSV token database. */
  private static native long newNativeDetokenizerCsv(byte[] database);

  /** Deletes the detokenizer object with the provided handle, which MUST be valid. */
  private static native void deleteNativeDetokenizer(long handle);

  // Use non-static functions so this object has a reference held while the function is running,
  // which prevents finalize from running before the native functions finish.

  /**
   * Returns the detokenized version of the provided data, or null if detokenization failed.
   */
  @Nullable private native byte[] detokenizeNative(long handle, byte[] data);

  /**
   * Returns the String with nested tokenized messages decoded within it.
   */
  private native byte[] detokenizeTextNative(long handle, String data);
}
