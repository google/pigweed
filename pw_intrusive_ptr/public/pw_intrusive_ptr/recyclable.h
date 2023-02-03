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
#pragma once

#include <type_traits>

// pw::Recyclable<T>
//
// Notes:
//
// pw::Recyclable<T> is a mix-in class which allows users to control what
// happens to objects when they reach the end of their lifecycle, as determined
// by the Pigweed managed pointer classes.
//
// The general idea is as follows. A developer might have some sort of factory
// pattern where they hand out unique_ptr<>s or IntrusivePtr<>s to objects which
// they have created. When their user is done with the object and the managed
// pointers let go of it, instead of executing the destructor and deleting the
// object, the developer may want to "recycle" the object and use it for some
// internal purpose. Examples include...
//
// 1) Putting the object on some sort of internal list to hand out again
//    of the object is re-usable and the cost of construction/destruction
//    is high.
// 2) Putting the object into some form of deferred destruction queue
//    because users are either too high priority to pay the cost of
//    destruction when the object is released, or because the act of
//    destruction might involve operations which are not permitted when
//    the object is released (perhaps the object is released at IRQ time,
//    but the system needs to be running in a thread in order to properly
//    clean up the object)
// 3) Re-using the object internally for something like bookkeeping
//    purposes.
//
// In order to make use of the feature, users need to do two things.
//
// 1) Derive from pw::Recyclable<T>.
// 2) Implement a method with the signature "void pw_recycle()"
//
// When deriving from Recyclable<T>, T should be devoid of cv-qualifiers (even
// if the managed pointers handed out by the user's code are const or volatile).
// In addition, pw_recycle must be visible to pw::Recyclable<T>, either
// because it is public or because the T is friends with pw::Recyclable<T>.
//
// :: Example ::
//
// Some code hands out unique pointers to const Foo objects and wishes to
// have the chance to recycle them.  The code would look something like
// this...
//
// class Foo : public pw::Recyclable<Foo> {
// public:
//   // public implementation here
// private:
//   friend class pw::Recyclable<Foo>;
//   void pw_recycle() {
//     if (should_recycle())) {
//       do_recycle_stuff();
//     } else {
//       delete this;
//     }
//   }
// };
//
// Note: the intention is to use this feature with managed pointers,
// which will automatically detect and call the recycle method if
// present.  That said, there is nothing to stop users for manually
// calling pw_recycle, provided that it is visible to the code which
// needs to call it.

namespace pw {

// Default implementation of pw::Recyclable.
//
// Note: we provide a default implementation instead of just a fwd declaration
// so we can add a static_assert which will give a user a more human readable
// error in case they make the mistake of deriving from pw::Recyclable<const
// Foo> instead of pw::Recyclable<Foo>
template <typename T, typename = void>
class Recyclable {
  // Note: static assert must depend on T in order to trigger only when the
  // template gets expanded.  If it does not depend on any template parameters,
  // eg static_assert(false), then it will always explode, regardless of whether
  // or not the template is ever expanded.
  static_assert(
      std::is_same_v<T, T> == false,
      "pw::Recyclable<T> objects must not specify cv-qualifiers for T.  "
      "Derive from pw::Recyclable<Foo>, not pw::Recyclable<const Foo>");
};

namespace internal {

// Test to see if an object is recyclable.  An object of type T is considered to
// be recyclable if it derives from pw::Recyclable<T>
template <typename T>
inline constexpr bool has_pw_recycle_v =
    std::is_base_of_v<::pw::Recyclable<std::remove_cv_t<T>>, T>;

template <typename T>
inline void recycle(T* ptr) {
  static_assert(has_pw_recycle_v<T>, "T must derive from pw::Recyclable");
  Recyclable<std::remove_cv_t<T>>::pw_recycle_thunk(
      const_cast<std::remove_cv_t<T>*>(ptr));
}

}  // namespace internal

template <typename T>
class Recyclable<T, std::enable_if_t<std::is_same_v<std::remove_cv_t<T>, T>>> {
 private:
  friend void ::pw::internal::recycle<T>(T*);
  friend void ::pw::internal::recycle<const T>(const T*);

  static void pw_recycle_thunk(T* ptr) {
    static_assert(std::is_same_v<decltype(&T::pw_recycle), void (T::*)(void)>,
                  "pw_recycle() methods must be non-static member functions "
                  "with the signature 'void pw_recycle()', and be visible to "
                  "pw::Recyclable<T> (either because they are public, or "
                  "because of friendship).");
    ptr->pw_recycle();
  }
};

}  // namespace pw
