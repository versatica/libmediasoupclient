#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}

current_dir_name=${PROJECT_PWD##*/}
if [ "${current_dir_name}" != "libsdptransform" ] ; then
	echo ">>> [ERROR] $(basename $0) must be called from libsdptransform/ root directory" >&2
	exit 1
fi

if [ "$1" == "build" ]; then
	# Rebuild.
	rm -rf build/ bin/
	cmake . -Bbuild
fi

# Compile.
cmake --build build

# Run test.
./build/test/test_sdptransform
