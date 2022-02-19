#!/bin/bash
# ______________________________________________________________________________
#
#  Convert images to headers
#  Used by build.sh
# ______________________________________________________________________________
#

# Default build options, override options from the command line

# Executable name, extension is added depending on target platform.
[[ "$NAME" = "" ]] && NAME="png2h"

# Compiler flags.
[[ "$FLAGS" = "" ]] && FLAGS=""

RELEASEFLAGS=""
DEBUGFLAGS="-O0 -g -Wall -Wextra -Wpedantic"

# ______________________________________________________________________________
#
#  Compile
# ______________________________________________________________________________
#
TYPEFLAGS=$RELEASEFLAGS
[[ "$DEBUG" != "" ]] && TYPEFLAGS=$DEBUGFLAGS

case `uname` in
	"Windows_NT")
		CC="x86_64-w64-mingw32-gcc"
		EXT=".exe"
		PLATFORM="PLATFORM_DESKTOP"
		TARGETFLAGS="-lopengl32 -lgdi32 -lwinmm -Wl,--subsystem,windows"
		;;

	"Linux")
		CC="gcc"
		PLATFORM="PLATFORM_DESKTOP"
		TARGETFLAGS="-lGL -lm -lpthread -ldl -lrt -lX11"
		;;

	*)
		echo "Unsupported OS `uname`"
		exit 1
		;;
esac

$CC src/png2h.c -Iinclude -Llib/`uname` -o $NAME$EXT \
	-lraylib -D$PLATFORM $FLAGS $TYPEFLAGS $TARGETFLAGS

./$NAME$EXT assets/splash.png assets/splash_web.png assets/tileset.png assets/icon.png assets/font.png