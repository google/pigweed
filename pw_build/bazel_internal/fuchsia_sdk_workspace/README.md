This `fuchsia_sdk_workspace` directory is a temporary location for Fuchsia Bazel SDK workspace rules to facilitate soft transitions.

TODO(b/363328762): Delete these once we can consume `@fuchsia_clang` and `@fuchsia_products` directly from CIPD.

We should not try to load workspace rules from the Bazel SDK itself, since that will cause the Bazel SDK to be unconditionally fetched.
