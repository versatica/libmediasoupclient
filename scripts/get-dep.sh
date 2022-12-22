#!/usr/bin/env bash

set -e

PROJECT_PWD=${PWD}
DEP=$1

current_dir_name=${PROJECT_PWD##*/}
if [ "${current_dir_name}" != "libmediasoupclient" ] && [ "${current_dir_name}" != "v3-libmediasoupclient" ] ; then
	echo ">>> [ERROR] $(basename $0) must be called from libmediasoupclient/ root directory" >&2
	exit 1
fi

function get_dep()
{
	GIT_REPO="$1"
	GIT_TAG="$2"
	DEST="$3"
	ADD_TO_REPO="$4"

	echo ">>> [INFO] getting dep '${DEP}' ..."

	if [ -d "${DEST}" ] ; then
		echo ">>> [INFO] deleting ${DEST} ..."
		git rm -rf --ignore-unmatch ${DEST} > /dev/null
		rm -rf ${DEST}
	fi

	echo ">>> [INFO] cloning ${GIT_REPO} ..."
	git clone ${GIT_REPO} ${DEST}

	cd ${DEST}

	echo ">>> [INFO] setting '${GIT_TAG}' git tag ..."
	git checkout --quiet ${GIT_TAG}
	set -e

	if [ "${ADD_TO_REPO}" == "YES" ] ; then
		echo ">>> [INFO] adding dep source code to the repository ..."
		rm -rf .git
		git add .
	fi

	echo ">>> [INFO] got dep '${DEP}'"

	cd ${PROJECT_PWD}
}

function get_libsdptransform()
{
	GIT_REPO="https://github.com/ibc/libsdptransform.git"
	GIT_TAG="1.2.10"
	DEST="deps/libsdptransform"
	ADD_TO_REPO="YES"

	get_dep "${GIT_REPO}" "${GIT_TAG}" "${DEST}" "${ADD_TO_REPO}"
}

function get_catch()
{
	GIT_REPO="https://github.com/philsquared/Catch.git"
	GIT_TAG="v3.2.1"
	DEST="deps/catch"
	ADD_TO_REPO="NO"

	get_dep "${GIT_REPO}" "${GIT_TAG}" "${DEST}" "${ADD_TO_REPO}"

	# Catch2 v3 is no longer a single header library and we should build it as a
	# proper (static) library. May do it in the future.
	# Doc: https://github.com/catchorg/Catch2/blob/devel/docs/migrate-v2-to-v3.md#migrating-from-v2-to-v3

	echo ">>> [INFO] copying include file to test/include/ directory ..."
	cp ${DEST}/extras/catch_amalgamated.hpp test/include/

	echo ">>> [INFO] copying source file to test/ directory ..."
	cp ${DEST}/extras/catch_amalgamated.cpp test/src/
}

case "${DEP}" in
	'-h')
		echo "Usage:"
		echo "  ./scripts/$(basename $0) [libsdptransform|catch]"
		echo
		;;
	libsdptransform)
		get_libsdptransform
		;;
	catch)
		get_catch
		;;
	*)
		echo ">>> [ERROR] unknown dep '${DEP}'" >&2
		exit 1
esac

if [ $? -eq 0 ] ; then
	echo ">>> [INFO] done"
else
	echo ">>> [ERROR] failed" >&2
	exit 1
fi
