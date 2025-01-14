.. _module-pw_crypto:

=========
pw_crypto
=========
.. pigweed-module::
   :name: pw_crypto

A set of safe (read: easy to use, hard to misuse) crypto APIs.

The following crypto services are provided by this module.

1. Hashing a message with `SHA256`_.
2. Verifying a digital signature signed with `ECDSA`_ over the NIST P256 curve.
3. Many more to come ...

------
SHA256
------
1. Obtaining a oneshot digest.

.. code-block:: cpp

   #include "pw_crypto/sha256.h"

   std::byte digest[32];
   if (!pw::crypto::sha256::Hash(message, digest).ok()) {
     // Handle errors.
   }

   // The content can also come from a pw::stream::Reader.
   if (!pw::crypto::sha256::Hash(reader, digest).ok()) {
     // Handle errors.
   }

2. Hashing a long, potentially non-contiguous message.

.. code-block:: cpp

   #include "pw_crypto/sha256.h"

   std::byte digest[32];

   if (!pw::crypto::sha256::Sha256()
       .Update(chunk1).Update(chunk2).Update(chunk...)
       .Final().ok()) {
     // Handle errors.
   }

-----
ECDSA
-----
1. Verifying a digital signature signed with ECDSA over the NIST P256 curve.

.. code-block:: cpp

   #include "pw_crypto/sha256.h"

   std::byte digest[32];
   if (!pw::crypto::sha256::Hash(message, digest).ok()) {
     // handle errors.
   }

   if (!pw::crypto::ecdsa::VerifyP256Signature(public_key, digest,
                                               signature).ok()) {
     // handle errors.
   }

2. Verifying a digital signature signed with ECDSA over the NIST P256 curve,
   with a long and/or non-contiguous message.

.. code-block:: cpp

   #include "pw_crypto/sha256.h"

   std::byte digest[32];

   if (!pw::crypto::sha256::Sha256()
       .Update(chunk1).Update(chunk2).Update(chunkN)
       .Final(digest).ok()) {
       // Handle errors.
   }

   if (!pw::crypto::ecdsa::VerifyP256Signature(public_key, digest,
                                               signature).ok()) {
       // Handle errors.
   }

---
AES
---

1. Encrypting a single AES 128-bit block.

.. warning::
  This is a low-level operation. Users should know exactly what they are doing
  and must ensure that this operation does not violate any safety bounds that
  more refined operations usually ensure.

.. code-block:: cpp

   #include "pw_crypto/aes.h"

   std::byte encrypted[16];

   if (!pw::crypto::unsafe::aes::EncryptBlock(key, message, encrypted).ok()) {
       // Handle errors.
   }

-------------
Configuration
-------------
The crypto services offered by pw_crypto can be backed by different backend
crypto libraries.

Mbed TLS
========
The `Mbed TLS project <https://www.trustedfirmware.org/projects/mbed-tls/>`_
is a mature and full-featured crypto library that implements cryptographic
primitives, X.509 certificate manipulation and the SSL/TLS and DTLS protocols.

The project also has good support for interfacing to cryptographic accelerators.

The small code footprint makes the project suitable and popular for embedded
systems.

To select the Mbed TLS backend, the MbedTLS library needs to be installed and
configured. If using GN, do,

.. code-block:: sh

   # Install and configure MbedTLS
   pw package install mbedtls
   gn gen out --args='
       dir_pw_third_party_mbedtls=getenv("PW_PACKAGE_ROOT")+"/mbedtls"
       pw_crypto_SHA256_BACKEND="//pw_crypto:sha256_mbedtls_v3"
       pw_crypto_ECDSA_BACKEND="//pw_crypto:ecdsa_mbedtls_v3"
       pw_crypto_AES_BACKEND="//pw_crypto:aes_mbedtls_v3"
   '

   ninja -C out

If using Bazel, add the Mbed TLS repository to your WORKSPACE and select
appropriate backends by adding them to your project's `platform
<https://bazel.build/extending/platforms>`_:

.. code-block:: python

   platform(
     name = "my_platform",
     flags = [
        "@pigweed//pw_crypto:sha256_backend=@pigweed//pw_crypto:sha256_mbedtls_backend",
        "@pigweed//pw_crypto:ecdsa_backend=@pigweed//pw_crypto:ecdsa_mbedtls_backend",
        "@pigweed//pw_crypto:aes_backend=@pigweed//pw_crypto:aes_mbedtls_backend",
        # ... other flags
      ],
   )

For optimal code size and/or performance, the Mbed TLS library can be configured
per product. Mbed TLS configuration is achieved by turning on and off MBEDTLS_*
options in a config.h file. See //third_party/mbedtls for how this is done.

``pw::crypto::sha256`` does not need any special configuration as it uses the
mbedtls_sha256_* APIs directly. However you can optionally turn on
``MBEDTLS_SHA256_SMALLER`` to further reduce the code size to from 3KiB to
~1.8KiB at a ~30% slowdown cost (Cortex-M4).

.. code-block:: c

   #define MBEDTLS_SHA256_SMALLER

``pw::crypto::ecdsa`` requires the following minimum configurations which yields
a code size of ~12KiB.

.. code-block:: c

   #define MBEDTLS_BIGNUM_C
   #define MBEDTLS_ECP_C
   #define MBEDTLS_ECDSA_C
   // The ASN1 options are needed only because mbedtls considers and verifies
   // them (in check_config.h) as dependencies of MBEDTLS_ECDSA_C.
   #define MBEDTLS_ASN1_WRITE_C
   #define MBEDTLS_ASN1_PARSE_C
   #define MBEDTLS_ECP_NO_INTERNAL_RNG
   #define MBEDTLS_ECP_DP_SECP256R1_ENABLED

.. _module-pw_crypto-boringssl:

BoringSSL
=========
The BoringSSL project (`source
<https://cs.opensource.google/boringssl/boringssl>`_, `GitHub mirror
<https://github.com/google/boringssl>`_) is a fork of OpenSSL maintained by
Google. It is not especially designed to be embedded-friendly, but it is used as
the SSL library in Chrome, Android, and other apps. It is likely better to use
another backend such as Mbed-TLS for embedded targets unless your project needs
BoringSSL specifically.

To use the BoringSSL backend with a GN project, it needs to be installed and
configured. To do that:

.. code-block:: sh

   # Install and configure BoringSSL
   pw package install boringssl
   gn gen out --args='
       dir_pw_third_party_boringssl=getenv("PW_PACKAGE_ROOT")+"/boringssl"
       pw_crypto_AES_BACKEND="//pw_crypto:aes_boringssl"
   '

   ninja -C out

If using Bazel, add the BoringSSL repository to your WORKSPACE or MODULE.bazel
and select appropriate backends by adding them to your project's `platform
<https://bazel.build/extending/platforms>`_:

.. code-block:: python

   platform(
     name = "my_platform",
     constraint_values = [
       "@pigweed//pw_aes:aes_boringssl_backend",
       # ... other constraint_values
     ],
   )

------------
Size Reports
------------
Below are size reports for each crypto service. These vary across
configurations.

.. include:: size_report

-------------
API reference
-------------
.. doxygenfunction:: pw::crypto::ecdsa::VerifyP256Signature(ConstByteSpan public_key, ConstByteSpan digest, ConstByteSpan signature)
.. doxygenfunction:: pw::crypto::sha256::Hash(ConstByteSpan message, ByteSpan out_digest)
.. doxygenfunction:: pw::crypto::sha256::Hash(stream::Reader& reader, ByteSpan out_digest)
.. doxygenvariable:: pw::crypto::sha256::kDigestSizeBytes
.. doxygenfunction:: pw::crypto::sha256::Sha256::Final(ByteSpan out_digest)
.. doxygenfunction:: pw::crypto::sha256::Sha256::Update(ConstByteSpan data)
.. doxygenenum::     pw::crypto::sha256::Sha256State
