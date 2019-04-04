#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}
TEST_BINARY=""

current_dir_name=${PROJECT_PWD##*/}
if [ "${current_dir_name}" != "libmediasoupclient" ] ; then
	echo ">>> [ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2
	exit 1
fi

# Load common script.
. scripts/common.sh

if [ "$1" == "build" ]; then
	# Rebuild.
	rm -rf build/
	cmake . -Bbuild
fi

# Compile.
cmake --build build

if [ "${OS}" = "Darwin" ]; then
	TEST_BINARY=./build/test/test_mediasoupclient.app/Contents/MacOS/test_mediasoupclient
else
	TEST_BINARY=./build/test/test_mediasoupclient
fi

echo "runing binary: '${TEST_BINARY}'"

# Run test.
${TEST_BINARY} $@
