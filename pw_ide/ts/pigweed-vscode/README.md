# Pigweed Extension for Visual Studio Code

Build [Pigweed](https://pigweed.dev) projects efficiently and effectively in
Visual Studio Code!

Features include:

* High-quality C/C++ code intelligence for embedded systems projects using
  [clangd](https://clangd.llvm.org/) integrated directly with your project's
  Bazel build graph

* Bundled core Bazel tools, letting you get started immediately without the need
  to install global system dependencies

* Interactive browsing, building, and running Bazel targets

* Built-in tools for building firmware, flashing it to your device, and
  communicating with your device once it's running

## Getting Started

Just install this extension and open your Pigweed project. The extension will
prompt you to set up a few things on first run.

For more details, check out the [full documentation](https://pigweed.dev/pw_ide/guide/vscode).

## Found a bug?

Run `Pigweed: File Bug` to let us know!

## Pre-Release Channel

Between main channel releases, pre-release versions will be published off of
Pigweed's `main` branch. Features available in pre-release versions can be
discovered in Pigweed's [changelog](https://pigweed.dev/changelog.html) or in
the [list of merged changes](https://pigweed-review.googlesource.com/q/status:merged).

You can opt in to the pre-release version on the Pigweed extension page in the
extension marketplace. Be warned that features in the pre-release versions may
not entirely work, and pre-release versions may break existing functionality.

You can always revert back to the main channel version in the Pigweed extension
page.

## Developing

If you want to contribute to this extension, or just build it locally, refer
to the [development docs](https://pigweed.dev/pw_ide/guide/vscode/development.html).
