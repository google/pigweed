.. _module-pw_crypto:

pw_crypto
=========
A set of safe (read: easy to use, hard to misuse) crypto APIs.

The following crypto services are provided by this module.

1. Hashing a message with `SHA256`_.
2. Verifying a digital signature signed with `ECDSA`_ over the NIST P256 curve.
3. Many more to come ...

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

Configuration
-------------

The crypto services offered by pw_crypto can be backed by different backend
crypto libraries.

Mbed TLS
^^^^^^^^

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
      import("//build_overrides/pigweed_environment.gni")

      dir_pw_third_party_mbedtls=pw_env_setup_PACKAGE_ROOT+"/mbedtls"
      pw_crypto_SHA256_BACKEND="//pw_crypto:sha256_mbedtls_v3"
      pw_crypto_ECDSA_BACKEND="//pw_crypto:ecdsa_mbedtls_v3"
  '

  ninja -C out

If using Bazel, add the Mbed TLS repository to your WORKSPACE and select
appropriate backends by adding them to your project's `platform
<https://bazel.build/extending/platforms>`_:

.. code-block:: python

  platform(
    name = "my_platform",
     constraint_values = [
       "@pigweed//pw_crypto:sha256_mbedtls_backend",
       "@pigweed//pw_crypto:ecdsa_mbedtls_backend",
       # ... other constraint_values
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

Micro ECC
^^^^^^^^^

To select Micro ECC, the library needs to be installed and configured.

.. code-block:: sh

  # Install and configure Micro ECC
  pw package install micro-ecc
  gn gen out --args='
      import("//build_overrides/pigweed_environment.gni")

      dir_pw_third_party_micro_ecc=pw_env_setup_PACKAGE_ROOT+"/micro-ecc"
      pw_crypto_ECDSA_BACKEND="//pw_crypto:ecdsa_uecc"
  '

The default micro-ecc backend uses big endian as is standard practice. It also
has a little-endian configuration which can be used to slightly reduce call
stack frame use and/or when non pw_crypto clients use the same micro-ecc
with a little-endian configuration. The little-endian version of micro-ecc
can be selected with ``pw_crypto_ECDSA_BACKEND="//pw_crypto:ecdsa_uecc_little_endian"``

Note Micro-ECC does not implement any hashing functions, so you will need to use other backends for SHA256 functionality if needed.

Size Reports
------------

Below are size reports for each crypto service. These vary across
configurations.

.. include:: size_report
