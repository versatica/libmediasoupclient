#!/usr/bin/env bash

set -e

# Always run from the project root (parent of the demo/ directory).
cd "$(dirname "$0")/..";
OS="$(uname -s)"

# Resolve the demo binary path.
if [ "${OS}" = "Darwin" ]; then
	DEMO_BINARY=./build/demo/mediasoupclient_demo.app/Contents/MacOS/mediasoupclient_demo
else
	DEMO_BINARY=./build/demo/mediasoupclient_demo
fi

# --help: forward directly to the binary (no build needed).
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	if [ -f "${DEMO_BINARY}" ]; then
		exec "${DEMO_BINARY}" --help
	fi
	echo "Usage: $(basename $0) [rebuild] [options]"
	echo "       $(basename $0) --help"
	exit 0
fi

# rebuild: wipe the build directory before configuring.
if [ "$1" = "rebuild" ]; then
	echo "[INFO] rebuilding: removing build/"
	rm -rf build/
	shift
fi

# Always configure (no-op when the build dir and flags are unchanged).
echo "[INFO] configuring: cmake . -Bbuild [...]"
cmake . -Bbuild \
	-DLIBWEBRTC_INCLUDE_PATH:PATH=${PATH_TO_LIBWEBRTC_SOURCES} \
	-DLIBWEBRTC_BINARY_PATH:PATH=${PATH_TO_LIBWEBRTC_BINARY} \
	-DMEDIASOUPCLIENT_BUILD_DEMO="true" \
	-DCMAKE_CXX_FLAGS="-fvisibility=hidden" \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5

echo "[INFO] compiling: cmake --build build --target mediasoupclient_demo"
cmake --build build --target mediasoupclient_demo

echo "[INFO] running: ${DEMO_BINARY} $@"
exec "${DEMO_BINARY}" "$@"
