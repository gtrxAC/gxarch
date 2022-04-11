#!/bin/bash
# ______________________________________________________________________________
#
#  Build options for png2h
# ______________________________________________________________________________
#
# Executable name, extension is added depending on target platform.
NAME=png2h

# Files to compile. You can add multiple files by separating by spaces.
SRC="src/png2h.c"

# Platform, one of Windows_NT, Linux. Defaults to your OS.
# This can be set from the command line: TARGET=Windows_NT ./build.sh
[[ -z "$TARGET" ]] && TARGET=$(uname)

# Compiler flags.
# This can be set from the command line: FLAGS="-Ofast" ./build.sh
[[ -z "$FLAGS" ]] && FLAGS=""

# Compiler flags for release and debug mode
# To set debug mode, run: DEBUG=1 ./build.sh
RELEASE_FLAGS=""
DEBUG_FLAGS="-DDEBUG -O0 -g -Wall -Wextra -Wpedantic"

# ______________________________________________________________________________
#
#  Convert images to headers
#  Used by build.sh
# ______________________________________________________________________________
#
# Add release or debug flags
if [[ -z "$DEBUG" ]]; then
	FLAGS="$FLAGS $RELEASE_FLAGS"
else
	FLAGS="$FLAGS $DEBUG_FLAGS"
fi

# Run the setup if the project hasn't been set up yet
[[ -e lib/$TARGET ]] || ./setup.sh

# Build options for each target
case "$TARGET" in
	"Windows_NT")
		# To build for 32-bit, set ARCH to i686 in both build and setup.sh, then
		# run setup again
		ARCH="x86_64"
		CC="$ARCH-w64-mingw32-gcc"
		EXT=".exe"
		PLATFORM="PLATFORM_DESKTOP"
		TARGET_FLAGS="-lopengl32 -lgdi32 -lwinmm -Wl,--subsystem,windows"
		;;

	"Linux")
		CC="gcc"
		PLATFORM="PLATFORM_DESKTOP"
		TARGET_FLAGS="-lm -ldl -lpthread"
		;;

	*)
		echo "Unsupported platform $(uname)"
		exit 1
		;;
esac

# Don't run the project if build fails
set -e

$CC $SRC -Iinclude -Llib/$TARGET -o $NAME$EXT \
	-lraylib -D$PLATFORM $FLAGS $TARGET_FLAGS

./$NAME$EXT assets/splash.png assets/splash_web.png assets/tileset.png assets/icon.png assets/font.png