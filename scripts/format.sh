#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}

# Import utils // OS, NUM_CORES
. scripts/common.sh

if [ "${OS}" != "Darwin" ] && [ "${OS}" != "Linux" ] ; then
	echo "[ERROR] only available for MacOS and Linux" >&2

	exit 1
fi

current_dir_name=${PROJECT_PWD##*/}
if [ "${current_dir_name}" != "libmediasoupclient" ] && [ "${current_dir_name}" != "v3-libmediasoupclient" ] ; then
	echo "[ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2

	exit 1
fi

# Run clang-format -i on all source an include files.
echo "[INFO] running node_utils/node_modules/.bin/clang-format"

node_utils/node_modules/.bin/clang-format \
	-i --style=file \
	src/*.cpp \
	include/*.hpp
