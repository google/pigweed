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

// This file provides a Java Native Interface (JNI) version of the Detokenizer
// class. This facilitates using the tokenizer library from Java or other JVM
// languages. A corresponding Java class is provided in Detokenizer.java.

#include <jni.h>

#include <cstdint>
#include <cstring>

#include "pw_span/span.h"
#include "pw_tokenizer/detokenize.h"
#include "pw_tokenizer/token_database.h"

#define DETOKENIZER_JNI_METHOD(return_type, method) \
  extern "C" JNIEXPORT return_type JNICALL          \
      Java_dev_pigweed_pw_1tokenizer_Detokenizer_##method

namespace pw::tokenizer {
namespace {

Detokenizer* HandleToPointer(jlong handle) {
  Detokenizer* detokenizer = nullptr;
  std::memcpy(&detokenizer, &handle, sizeof(detokenizer));
  static_assert(sizeof(detokenizer) <= sizeof(handle));
  return detokenizer;
}

jlong PointerToHandle(Detokenizer* detokenizer) {
  jlong handle = 0;
  std::memcpy(&handle, &detokenizer, sizeof(detokenizer));
  static_assert(sizeof(handle) >= sizeof(detokenizer));
  return handle;
}

jbyteArray ByteArrayFromString(JNIEnv* env, const std::string& str) {
  const jsize size = static_cast<jsize>(str.size());
  jbyteArray array = env->NewByteArray(size);
  env->SetByteArrayRegion(
      array, 0, size, reinterpret_cast<const jbyte*>(str.data()));
  return array;
}

}  // namespace

static_assert(sizeof(jbyte) == 1u);

DETOKENIZER_JNI_METHOD(jlong, newNativeDetokenizerBinary)(JNIEnv* env,
                                                          jclass,
                                                          jbyteArray array) {
  jbyte* const data = env->GetByteArrayElements(array, nullptr);

  TokenDatabase tokens = TokenDatabase::Create(
      span(data, static_cast<size_t>(env->GetArrayLength(array))));
  Detokenizer* detok = new Detokenizer(tokens.ok() ? tokens : TokenDatabase());

  env->ReleaseByteArrayElements(array, data, 0);
  return PointerToHandle(detok);
}

DETOKENIZER_JNI_METHOD(jlong, newNativeDetokenizerCsv)(JNIEnv* env,
                                                       jclass,
                                                       jbyteArray csv) {
  jbyte* const data = env->GetByteArrayElements(csv, nullptr);

  auto result = Detokenizer::FromCsv(
      std::string_view(reinterpret_cast<const char*>(data),
                       static_cast<size_t>(env->GetArrayLength(csv))));

  Detokenizer* detok = result.ok() ? new Detokenizer(*std::move(result))
                                   : new Detokenizer(TokenDatabase());
  env->ReleaseByteArrayElements(csv, data, 0);
  return PointerToHandle(detok);
}

DETOKENIZER_JNI_METHOD(void,
                       deleteNativeDetokenizer)(JNIEnv*, jclass, jlong handle) {
  delete HandleToPointer(handle);
}

DETOKENIZER_JNI_METHOD(jbyteArray, detokenizeNative)(JNIEnv* env,
                                                     jobject,
                                                     jlong handle,
                                                     jbyteArray array) {
  jbyte* const data = env->GetByteArrayElements(array, nullptr);
  const jsize size = env->GetArrayLength(array);

  DetokenizedString result =
      HandleToPointer(handle)->Detokenize(data, static_cast<size_t>(size));

  env->ReleaseByteArrayElements(array, data, 0);

  return result.ok() ? ByteArrayFromString(env, result.BestString()) : nullptr;
}

DETOKENIZER_JNI_METHOD(jbyteArray, detokenizeTextNative)(JNIEnv* env,
                                                         jobject,
                                                         jlong handle,
                                                         jstring message) {
  const char* const data = env->GetStringUTFChars(message, nullptr);
  const jsize size = env->GetStringUTFLength(message);

  std::string result = HandleToPointer(handle)->DetokenizeText(std::string_view(
      reinterpret_cast<const char*>(data), static_cast<size_t>(size)));

  env->ReleaseStringUTFChars(message, data);
  return ByteArrayFromString(env, result);
}

}  // namespace pw::tokenizer
