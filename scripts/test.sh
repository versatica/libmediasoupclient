#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}
OS="$(uname -s)"

current_dir_name=${PROJECT_PWD##*/}

if [ "${current_dir_name}" != "libmediasoupclient" ] && [ "${current_dir_name}" != "v3-libmediasoupclient" ] ; then
	echo "[ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2

	exit 1
fi

# Rebuild everything.
if [ "$1" == "rebuild" ]; then
	echo "[INFO] rebuilding CMake project: cmake . -Bbuild [...]"

	rm -rf build/
	cmake . -Bbuild \
		-DLIBWEBRTC_INCLUDE_PATH:PATH=${PATH_TO_LIBWEBRTC_SOURCES} \
		-DLIBWEBRTC_BINARY_PATH:PATH=${PATH_TO_LIBWEBRTC_BINARY} \
		-DMEDIASOUPCLIENT_BUILD_TESTS="true" \
		-DCMAKE_CXX_FLAGS="-fvisibility=hidden"

	# Remove the 'rebuild' argument.
	shift
fi

# Compile.
echo "[INFO] compiling mediasoupclient and test_mediasoupclient: cmake --build build"

cmake --build build

# Run test.
if [ "${OS}" = "Darwin" ]; then
	TEST_BINARY=./build/test/test_mediasoupclient.app/Contents/MacOS/test_mediasoupclient
else
	TEST_BINARY=./build/test/test_mediasoupclient
fi

echo "[INFO] running tests: ${TEST_BINARY} $@"

${TEST_BINARY} $@
