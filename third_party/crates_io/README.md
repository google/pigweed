# Third Party Rust Crates

TODO: https://pwbug.dev/392963279 - Integrate these docs with the rest of
    Pigweed's docs

This directory is where third party crate dependencies are managed.  There are
two sets of crates: `./crates_no_std` and `./crates_std`.  These are `cargo`
based "null" Rust libraries that exist solely to express which crates to expose
to Bazel.  `./rust_crates` is a Bazel repository that exposes these crates to
Bazel.

## Adding a third party crate

1. Add the crate to either or both of `./crates_no_std` and `./crates_std`
   by running `cargo add <crate_name> --features <additional_features>` in the
   appropriate directory.
2. Run cargo deny:
   `(cd crates_std; cargo deny check) && (cd crates_no_std; cargo deny check)`

   * Install with `cargo install --locked cargo-deny` if not already installed.
3. Update `./rust_crates/aliases.bzl` by running
   `cargo run -- --config config.toml > rust_crates/aliases.bzl` in this
   directory.
