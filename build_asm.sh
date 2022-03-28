#!/bin/bash
# ______________________________________________________________________________
#
#  Build options for gxarch assembler
# ______________________________________________________________________________
#
# Executable name, extension is added depending on target platform.
NAME=gxasm

# Files to compile. You can add multiple files by separating by spaces.
SRC="asm/*.c"

# Platform, one of Windows_NT, Linux. Defaults to your OS.
# This can be set from the command line: TARGET=Windows_NT ./build.sh
[[ -z "$TARGET" ]] && TARGET=$(uname)

# Compiler flags.
# This can be set from the command line: FLAGS="-Ofast" ./build.sh
[[ -z "$FLAGS" ]] && FLAGS=""

# Compiler flags for release and debug mode
# To set debug mode, run: DEBUG=1 ./build_asm.sh
RELEASE_FLAGS="-Os -flto -s"
DEBUG_FLAGS="-DDEBUG -O0 -g -Wall -Wextra -Wpedantic"

# ______________________________________________________________________________
#
#  Compile gxarch assembler
# ______________________________________________________________________________
#
# Add release or debug flags
if [[ -z "$DEBUG" ]]; then
	FLAGS="$FLAGS $RELEASE_FLAGS"
else
	FLAGS="$FLAGS $DEBUG_FLAGS"
fi

# Build options for each target
case "$TARGET" in
	"Windows_NT")
		# To build for 32-bit, set ARCH to i686
		ARCH="x86_64"
		CC="$ARCH-w64-mingw32-gcc"
		EXT=".exe"
		;;

	"Linux")
		CC="gcc"
		TARGET_FLAGS="-lm"
		;;

	*)
		echo "Unsupported platform $TARGET"
		exit 1
		;;
esac

$CC $SRC -Iinclude -o $NAME$EXT $FLAGS $TARGET_FLAGS