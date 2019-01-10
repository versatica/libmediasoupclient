#!/usr/bin/env bash

set -e

# OS name.
OS="$(uname -s)"

# Number of cores.
NUM_CORES=

case "${OS}" in
	Linux*)     NUM_CORES=$(nproc);;
	Darwin*)    NUM_CORES=$(sysctl -n hw.ncpu);;
	*)          NUM_CORES=1;;
esac
