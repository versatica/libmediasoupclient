#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}

current_dir_name=${PROJECT_PWD##*/}

if [ "${current_dir_name}" != "libsdptransform" ] ; then
	echo "[ERROR] $(basename $0) must be called from libsdptransform/ root directory" >&2

	exit 1
fi

# Rebuild everything.
if [ "$1" == "rebuild" ]; then
	echo "[INFO] rebuilding CMake project: cmake . -Bbuild [...]"

	rm -rf build/ bin/
	cmake . -Bbuild

	# Remove the 'rebuild' argument.
	shift
fi

# Compile.
echo "[INFO] compiling sdptransform and test_sdptransform: cmake --build build"

cmake --build build

# Run test.
TEST_BINARY=./build/test/test_sdptransform

echo "[INFO] running tests: ${TEST_BINARY} $@"

${TEST_BINARY} $@
