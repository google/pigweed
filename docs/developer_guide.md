# Pigweed Developer Guide

Pigweed is an assortment of tooling and libraries for embedded projects. One
of the key goals is that you may chose to use things from Pigweed in an *à la
carte* fashion. Pigweed is a bundle of helper libraries, but it doesn't provide
an application image that can be run on a device. Because of this, the binaries
produced when building upstream Pigweed are unit tests (which can be directly
flashed to a device and run) and host tools.

If you haven't already, you'll need to go through the [Pigweed setup](setup.md)
process to ensure you have the necessary tools before running the commands
throughout this guide.

## Pigweed Environment

After going through the initial setup process, your current terminal will be in
the Pigweed development environment that provides all the tools you should need
to develop on Pigweed. If leave that session, you can activate the
environment in a new session with the following command:

**Linux/macOS**
```bash
$ . pw_env_setup/env_setup.sh
```

**Windows**
```batch
> call pw_env_setup\env_setup.bat
```

This will provide you with GN, Ninja, a host and Cortex-M compiler,
clang-format, a recent version of Python, a kitted out Python virtual
environment (venv), and more—all without messing with your current system
configuration or requiring manual installation.

## Building Pigweed With GN

Pigweed's primary build system is GN/Ninja based. There are
CMake and Bazel builds in-development, but they are incomplete and don't have
feature parity with the GN build. We strongly recommend you stick to the GN
build system.

GN (Generate Ninja) just does what it says on the tin; GN generates
[Ninja](https://ninja-build.org/) build files.

```bash
$ gn gen out/host
```

The default GN build configuration is set up to build with the host as the
target (more on targets later). Note that `out/host` is the output directory
that Ninja files are generated to, it doesn't imply any configuration. Now that
the build files have been generated, you can begin the build using Ninja:

```bash
$ ninja -C out/host
```

Whenever you make changes to the source or build files, you only need to run
`ninja`. Running GN again is only necessary if you delete your build directory
and need to generate a new one.

No build system is perfect, so you might have the desire to clean your build
directory if you're doubting the correctness of the build. Doing a clean build
is relatively simple: delete the build directory `out/[target]`, and
re-generate it using `gn gen`.

## pw_watch

Manually invoking the build can be tedious, though. You need to switch window
focus and re-run Ninja. Fortunately, one of Pigweed's modules addresses this!
pw_watch is a module that watches the Pigweed repository for changes to files
with certain extensions.

To start pw_watch, simply run `pw watch`:

![build example using pw watch](images/pw_watch_build_demo.gif)

Whenever you modify files, pw watch will trigger a build for all the build
subdirectories found in `out/`. Try it, you might be surprised how much time it
can save you!

## Running Unit Tests

Fun fact, you've been running the unit tests already! Host builds automatically
build and run the unit tests. Unit tests err on the side of being quiet in the
success case, and only output test results when there's a failure. If you want
to try this out, modify one of the tests to fail:

![example test failure using pw watch](images/pw_watch_test_demo.gif)

Running tests as part of the build isn't particularly expensive because GN
caches passing tests. Only affected tests are re-built/rerun on a code change
(thanks to GN's dependency resolution).

## Selecting a Build Target

As mentioned previously, Pigweed builds for host by default. This builds unit
tests and any host tools. In the context of Pigweed, a Pigweed "target" is a
build configuration that sets a toolchain, default library configurations,
and more to result in binaries that may be run natively on the target.

Here are some targets of interest:

 - **Host** (//targets/host/host.gni): Builds unit tests and host tools.

 - **Docs** (//targets/docs/target_config.gni): Builds these docs! This requires
   a build target because of the binary size reporting automatically generated
   with the documents.

 - **STM32F429I-DISC1** (//targets/stm32f429i-disc1/target_config.gni): Builds
   unit tests for STMicroelectronics's Discovery development board (MPN:
   STM32F429I-DISC1). This is the device Pigweed uses for upstream development.
   It's a relatively easy to acquire Cortex-M4 development board, and has an
   integrated STLink to simplify flashing and debugging of the device.

Rather than having tens or hundreds of exposed GN arguments that configure the
build and are easy to accidentally modify, we use target configuration files
that only expose arguments relevant to the target. This also makes it easier
to check in changes to target configurations.

**Setting the build target**

To set the target configuration for a build, set the `pw_target_config` GN build
arg to point to the target configuration `.gni` file you wish to use.

In this example, we generating Ninja build files for the stm32f429i-disc1.
```bash
$ gn gen --args='pw_target_config = "//targets/stm32f429i-disc1/target_config.gni"' out/disco
```

At this time, you may either run `ninja -C out/disco` or restart pw_watch to
build for the STM32F429I-DISC1.

## Running Tests on a Device

Even though we've verified tests pass on the host, it's critical to verify the
same with the board. We've encountered some unexpected bugs that can only be
found by running the unit tests directly on the device. This section will help
you get set up to enable on-device testing integrated into the Ninja build
(pw_watch comptible!) in three simple steps.

### 1. Connect Device(s)
Connect any number of STM32F429I-DISC1 boards to your computer using the mini
USB port on the board (**not** the micro USB). Pigweed will automatically detect
the boards and distribute the tests across the devices. More boards = faster
tests!

![](images/stm32f429i-disc1_connected.jpg)

### 2. Launch Test Server
To allow Ninja to run tests on an arbitrary number of devices, Ninja will send
test requests to a server running in the background. Launch the server in
another window using the command below (remember, you'll need to activate the
Pigweed env first).

```shell
  $ stm32f429i_disc1_test_server
```

**Note:** If you attach or detach any more boards to your workstation you'll
need to relaunch this server.

### 3. Configure GN

We can tell GN to use the testing server by enabling a build arg specific to
the stm32f429i-disc1.

```shell
$ gn args out/disco
# Modify and save the args file to tell GN to run on-device unit tests.
pw_use_test_server = true
```

### Done!

Whenever you make code changes and trigger a build, all the affected unit tests
will be run across the attached boards!

## Next steps

This concludes the introduction to developing with Pigweed. If you'd like to see
more of what Pigweed has to offer, feel free to dive into the per-module
documentation. If you run into snags along the way, please [file
bugs](https://bugs.chromium.org/p/pigweed/issues/entry)!
