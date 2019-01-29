#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}

# Import utils // OS, NUM_CORES
. scripts/common.sh

if [ "${OS}" != "Darwin" ] && [ "${OS}" != "Linux" ] ; then
	echo "Only available for MacOS and Linux"
	exit 1;
fi

current_dir_name=${PROJECT_PWD##*/}
if [ "${current_dir_name}" != "libmediasoupclient" ] ; then
	echo ">>> [ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2
	exit 1
fi

# Excluded checks.
EXCLUDED_CHECKS="\
-google-runtime-references,\
-llvm-header-guard,\
-misc-throw-by-value-catch-by-reference,\
-readability-function-size
"
# Filterer files/lineeing checked.
# LINE_FILTER="[\
# {\"name\":\"FakeVideoCapturer.cpp, \"lines\":[[1, 1000]]}\
# ]"
LINE_FILTER=

echo "line_filter: ${LINE_FILTER}"

# Checks to be performed.
CHECKS=""
# If certains checks are defined, just run those.
if [ ! -z ${MSC_TIDY_CHECKS} ] ; then
	CHECKS="-*,${MSC_TIDY_CHECKS}"
# Otherwise run all the checks except the excluded ones.
else
	CHECKS="${EXCLUDED_CHECKS}"
fi

# Whether replacements should be done.
FIX=${MSC_TIDY_FIX:=}
if [ ! -z ${MSC_TIDY_FIX} ] ; then
	FIX="-fix"
fi

HEADER_FILTER_REGEX="(Consumer.hpp|Device.hpp|Exception.hpp|Handler.hpp|Logger.hpp|PeerConnection.hpp|Producer.hpp|Transport.hpp|Utils.hpp|ortc.hpp|sdp/RemoteSdp.hpp|sdp/Utils.hpp)"

BIN_PATH="utils/node_modules/.bin"

# Generate compile_commands.json.
pushd build
cmake -DMSC_LOG_DEV=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
mv compile_commands.json ../
popd

# Run clang-tidy.py.
scripts/clang-tidy.py\
 -clang-tidy-binary=${BIN_PATH}/clang-tidy\
 -clang-apply-replacements-binary=${BIN_PATH}/clang-apply-replacements\
 -header-filter=${HEADER_FILTER_REGEX}\
 -line-filter=${LINE_FILTER}\
 -p=.\
 -j=${NUM_CORES}\
 -checks=${CHECKS}\
 ${FIX}
