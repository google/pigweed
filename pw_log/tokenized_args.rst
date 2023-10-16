.. _module-pw_log-tokenized-args:

-----------------------
Tokenized log arguments
-----------------------
The ``pw_log`` facade is intended to make the logging backend invisible to the
user, but some backend features require additional frontend support,
necessitating a break in the abstraction. One of these features is the logging
of nested token arguments in tokenized logs, because only the user is able to
know which log arguments can be tokens (see :ref:`module-pw_tokenizer` for
context on tokenization, and :ref:`module-pw_log_tokenized` for an example of
a tokenized logging backend). Arguments that have already been tokenized are
just unsigned integers, and not all strings can be compile-time constants, so
users must be provided with a way of manually marking token arguments.

To this end, ``pw_log/tokenized_args.h`` aliases macros from ``pw_tokenizer``
to enable logging nested tokens when the active backend uses tokenization.
These alias macros revert to plain string logging otherwise, allowing projects
to take advantage of nested token logging without breaking readable logs when
the project is built with a different logging backend. To support logging
nested token arguments, a ``pw_log`` backend must add an empty file
``log_backend_uses_pw_tokenizer.h`` under ``public_overrides/pw_log_backend/``.

Although the detokenizing backend accepts several different numeric bases, the
macros currently only support formatting nested tokens in hexadecimal, which
affects how the arguments appear in final logs if they cannot be detokenized
for any reason. Undetokenized tokens will appear inline as hex integers
prefixed with ``$#``, e.g. ``$#34d16466``.

.. doxygendefine:: PW_LOG_TOKEN_TYPE
.. doxygendefine:: PW_LOG_TOKEN
.. doxygendefine:: PW_LOG_TOKEN_EXPR
.. doxygendefine:: PW_LOG_TOKEN_FMT

Example usage with inline string arguments:

.. code-block:: cpp

  #include "pw_log/log.h"
  #include "pw_log/tokenized_args.h"

  // bool active_
  PW_LOG_INFO("Component is " PW_LOG_TOKEN_FMT(),
              active_ ? PW_LOG_TOKEN_EXPR("active")
                      : PW_LOG_TOKEN_EXPR("idle"));

Example usage with enums:

.. code-block:: cpp

  #include "pw_log/log.h"
  #include "pw_log/tokenized_args.h"

  namespace foo {

  enum class Color { kRed, kGreen, kBlue };

  PW_LOG_TOKEN_TYPE ColorToToken(Color color) {
    switch (color) {
      case Color::kRed:
        return PW_LOG_TOKEN_EXPR("kRed");
      case Color::kGreen:
        return PW_LOG_TOKEN_EXPR("kGreen");
      case Color::kBlue:
        return PW_LOG_TOKEN_EXPR("kBlue");
      default:
        return PW_LOG_TOKEN_EXPR("kUnknown");
    }
  }

  } // namespace foo

  void LogColor(foo::Color color) {
    PW_LOG("Color: [" PW_LOG_TOKEN_FMT() "]", color)
  }

