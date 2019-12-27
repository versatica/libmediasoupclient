#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}
OS="$(uname -s)"
TEST_BINARY=""

current_dir_name=${PROJECT_PWD##*/}

if [ "${current_dir_name}" != "libmediasoupclient" ] && [ "${current_dir_name}" != "v3-libmediasoupclient" ] ; then
	echo "[ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2

	exit 1
fi

# Rebuild everything.
if [ "$1" == "build" ]; then
	echo "[INFO] rebuilding everything: cmake . -Bbuild [...]"

	rm -rf build/
	cmake . -Bbuild \
		-DLIBWEBRTC_INCLUDE_PATH:PATH=${PATH_TO_LIBWEBRTC_SOURCES} \
		-DLIBWEBRTC_BINARY_PATH:PATH=${PATH_TO_LIBWEBRTC_BINARY} \
		-DMEDIASOUPCLIENT_BUILD_TESTS="true" \
		-DCMAKE_CXX_FLAGS="-fvisibility=hidden"

	# Remove the 'build' argument.
	shift
fi

# Compile.
echo "[INFO] compiling: cmake --build build"

cmake --build build

if [ "${OS}" = "Darwin" ]; then
	TEST_BINARY=./build/test/test_mediasoupclient.app/Contents/MacOS/test_mediasoupclient
else
	TEST_BINARY=./build/test/test_mediasoupclient
fi

# Run test.
echo "[INFO] running test: ${TEST_BINARY} $@"

${TEST_BINARY} $@
