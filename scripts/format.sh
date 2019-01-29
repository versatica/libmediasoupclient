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

# Run clang-format -i on 'include' and 'src' folders.
for dir in "include src test/*.test.* test/**/*.test.*"; do
    find ${dir} \
         \( -name '*.cpp' \
         -o -name '*.hpp' \) \
         -exec 'utils/node_modules/.bin/clang-format' -i '{}' \;
    popd &>/dev/null
done
