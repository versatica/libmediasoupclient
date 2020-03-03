#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}
OS="$(uname -s)"
NUM_CORES=1

current_dir_name=${PROJECT_PWD##*/}

if [ "${current_dir_name}" != "libmediasoupclient" ] && [ "${current_dir_name}" != "v3-libmediasoupclient" ] ; then
	echo "[ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2

	exit 1
fi

case "${OS}" in
	Linux*)  NUM_CORES=$(nproc);;
	Darwin*) NUM_CORES=$(sysctl -n hw.ncpu);;
	*)       NUM_CORES=1;;
esac

if [ "${OS}" != "Darwin" ] && [ "${OS}" != "Linux" ] ; then
	echo "[ERROR] only available for MacOS and Linux" >&2

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
# {\"name\":\"file.cpp, \"lines\":[[1, 1000]]}\
# ]"
LINE_FILTER=""

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

# Exclude dependencies, files which absolute path containins /deps/.
FILES="^((?!\/deps\/).)*$"

HEADER_FILTER_REGEX="(Consumer.hpp|Device.hpp|Handler.hpp|Logger.hpp|MediaSoupClientErrors.hpp|PeerConnection.hpp|Producer.hpp|Transport.hpp|mediasoupclient.hpp|ortc.hpp|scalabilityMode.hpp|sdp/RemoteSdp.hpp|sdp/Utils.hpp)"

BIN_PATH="node_modules/.bin"

# Generate compile_commands.json.
pushd build
cmake -DMSC_LOG_DEV=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
mv compile_commands.json ../
popd

# Run clang-tidy.py.
echo "[INFO] running scripts/clang-tidy.py"

scripts/clang-tidy.py \
	-clang-tidy-binary=${BIN_PATH}/clang-tidy \
	-clang-apply-replacements-binary=${BIN_PATH}/clang-apply-replacements \
	-header-filter=${HEADER_FILTER_REGEX} \
	-line-filter=${LINE_FILTER} \
	-p=. \
	-j=${NUM_CORES} \
	-checks=${CHECKS} \
	${FIX} \
	files $FILES
