#!/bin/sh
#
# Use gcc to build the source files and put them into the build directory
#

SOURCES=$(find src -name '*.c')
OBJECTS=

mkdir build >/dev/null 2>&1

for s in $SOURCES
do
	o=build/${s:4}.o
	OBJECTS="$OBJECTS $o"
	if [ $s -nt $o ]
	then
		echo Building $s
		gcc -c $s -o $o -Iinclude
	fi
done

echo Building $OBJECTS
gcc -g $OBJECTS -o build/out -lncurses
