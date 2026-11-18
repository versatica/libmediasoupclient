# Building

This document is intended for libmediasoupclient developers.

## Requirements

- CMake >= 3.14 and a C++20 compiler.
- libwebrtc sources and its prebuilt static library. See the `build-libwebrtc` GitHub Actions workflow for the supported revision and build arguments.
- Node.js >= 24, `clang-format` and `clang-tidy` for the code style and static analysis tasks below.

## CMake

Both the libwebrtc sources path and the libwebrtc binary path must be given to CMake:

```bash
cmake . -Bbuild \
  -DLIBWEBRTC_INCLUDE_PATH:PATH=/path/to/webrtc-checkout/src \
  -DLIBWEBRTC_BINARY_PATH:PATH=/path/to/webrtc-checkout/src/out/Release/obj \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build
```

"LIBWEBRTC_BINARY_PATH" must be the directory holding the libwebrtc static library, not the library file itself.

`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` generates `build/compile_commands.json`, which is required by `npm run tidy` below.

### Installing

```bash
cmake --install build --prefix /usr/local
```

It installs the static library into `lib/` and the whole `include/` tree (preserving the `sdp/` subfolder) into `include/mediasoupclient/`.

Applications need **both** of these include directories:

- `<prefix>/include`, so that they can use `#include <mediasoupclient/mediasoupclient.hpp>`.
- `<prefix>/include/mediasoupclient`, because the headers include each other with their folder (`sdp/RemoteSdp.hpp` does `#include "sdp/MediaSection.hpp"`), which keeps those includes unambiguous if another folder ever holds a header with the same name.

Both are already declared by the `mediasoupclient` CMake target, so a project that links it within the same build gets them automatically.

The headers of libwebrtc and of libsdptransform are part of the public interface too, so applications must have them available as well.

### CMake options

- "MEDIASOUPCLIENT_BUILD_TESTS": Build unit tests. Disabled by default.
- "MEDIASOUPCLIENT_BUILD_DEMO": Build demo app. Disabled by default.
- "MEDIASOUPCLIENT_LOG_TRACE": Enable `MSC_LOG_TRACE`. Disabled by default.
- "MEDIASOUPCLIENT_LOG_DEV": Enable `MSC_LOG_DEV`. Disabled by default.

Check the meaning of useful macros in the `include/Logger.hpp` header file if you want to enable tracing or other debug information.

## NPM scripts

The `package.json` file in the main folder includes the following scripts. They require its dependencies to be installed:

```bash
npm ci
```

### `npm run lint`

Validates libmediasoupclient C++ files using [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and rules in `.clang-format`.

**Requirements:**

- A specific version of `clang-format` is required. See [Install clang-format](#install-clang-format).
- `clang-format-VERSION` or `clang-format` (corresponding to the required version) must be in the `PATH`. If not, add it before running the command.

### `npm run format`

Same as `npm run lint` but it rewrites the files instead of just validating them.

### `npm run tidy`

Runs [clang-tidy](http://clang.llvm.org/extra/clang-tidy) and performs C++ code checks following `.clang-tidy` rules.

**Requirements:**

- A specific version of `clang-tidy` is required. See [Install clang-tidy](#install-clang-tidy).
- `clang-tidy-VERSION` or `clang-tidy` (corresponding to the required version) must be in the `PATH`. If not, add it before running the command. Same for other `clang-tidy` related executables such as `run-clang-tidy` and `clang-apply-replacements`.
- The project must have been configured with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, since the task reads `build/compile_commands.json`.

**Environment variables:**

- "BUILD_DIR": Optional. Build directory holding the `compile_commands.json` to use. Defaults to `build`.
- "LIBMEDIASOUPCLIENT_TIDY_CHECKS": Optional. Comma separated list of checks. Overrides the checks defined in `.clang-tidy` file.
- "LIBMEDIASOUPCLIENT_TIDY_FILES": Optional. Space separated source file paths to process. All `.cpp` files will be processed by default.
  - File paths must be relative to the main folder.
  - File paths can use [glob](https://github.com/isaacs/node-glob) syntax. Example: `"src/sdp/**/*.cpp"`.

**Usage example in macOS:**

```bash
PATH="/opt/homebrew/opt/llvm/bin/:$PATH" SDKROOT=$(xcrun --show-sdk-path) npm run tidy
```

"SDKROOT" is needed in macOS, otherwise every analyzed file fails with errors such as:

```
error: 'algorithm' file not found [clang-diagnostic-error]
error: 'optional' file not found [clang-diagnostic-error]
```

The reason is that CMake records the system compiler (`/usr/bin/c++`) in `compile_commands.json` without an `-isysroot` flag, since Apple clang resolves the SDK implicitly. `clang-tidy` builds its driver out of that compiler path, so it searches for the headers under `/usr`, where there is neither libc++ nor the SDK. "SDKROOT" tells it where the SDK is.

### `npm run tidy:fix`

Same as `npm run tidy` but it also applies fixes.

Some checks (such as `misc-const-correctness`) produce new warnings once previous ones are fixed, so it may need to be run more than once. Fixes within macros are never applied, those must be fixed by hand in the macro definition.

### `npm run normalize-compile-commands`

Rewrites the paths of `compile_commands.json` so that third party `clang-tidy` runners can resolve them. Just needed by CI.

## Tests

```bash
./scripts/test.sh          # build and run
./scripts/test.sh rebuild  # clean, rebuild and run
```

Read the [test/README.md](../test/README.md) documentation for detailed information.

## Linter configuration files

`.clang-format` and `.clang-tidy` are kept in sync with [mediasoup](https://github.com/versatica/mediasoup)'s `worker` folder. Do not diverge from them: update the files there first and copy them here. There are just a few intentional exceptions, all of them documented in the files themselves.

In addition, `test/.clang-tidy` inherits the root configuration and disables the checks that [Catch2](https://github.com/catchorg/Catch2) macros trigger.

## Install clang-format

A specific `clang-format` version is required to be installed in the system, which is defined in [clang-scripts.mjs](../scripts/clang-scripts.mjs).

macOS:

```bash
brew install clang-format@VERSION
```

Linux:

```bash
apt-get install clang-format-VERSION
```

## Install clang-tidy

A specific `clang-tidy` version is required to be installed in the system, which is defined in [clang-scripts.mjs](../scripts/clang-scripts.mjs).

macOS:

```bash
brew install llvm@VERSION
```

Linux:

```bash
apt-get install clang-tidy-VERSION
```
