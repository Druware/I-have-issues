#!/bin/sh
#
# run-tests.sh -- build and run the issueskit test suite without make.
#
# Works on Haiku, macOS and Linux: the library under test is pure C++17 with no
# platform toolkit, so all this needs is a C++17 compiler.
#
#   ./run-tests.sh              use the first available compiler
#   CXX=g++ ./run-tests.sh      pick one explicitly
#
# Exits non-zero if the build fails or any assertion fails.

set -eu

cd "$(dirname "$0")"

if [ -z "${CXX:-}" ]; then
	for candidate in c++ g++ clang++; do
		if command -v "$candidate" >/dev/null 2>&1; then
			CXX="$candidate"
			break
		fi
	done
fi

if [ -z "${CXX:-}" ]; then
	echo "No C++ compiler found. Set CXX to one." >&2
	exit 1
fi

CXXFLAGS="${CXXFLAGS:--std=c++17 -Wall -Wextra -O1}"

# Globbed, so a new library source is picked up without editing this script.
LIB_SRCS=$(echo ../src/*.cpp)
TEST_SRCS=$(echo ./*.cpp)

if [ "$LIB_SRCS" = '../src/*.cpp' ]; then
	echo "No library sources found in ../src" >&2
	exit 1
fi

echo "Building with $CXX ..."

# shellcheck disable=SC2086
"$CXX" $CXXFLAGS -I../include $LIB_SRCS $TEST_SRCS -o issueskit-tests

echo
exec ./issueskit-tests ../../../apps/Apple/sample/Example.issues
