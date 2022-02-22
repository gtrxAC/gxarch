#!/bin/bash
_ () { [[ "${!1}" = "" ]] && export $1="$2"; }
# ______________________________________________________________________________
#
#  Convert images to headers
#  Used by build.sh
# ______________________________________________________________________________
#
#  Build options
#  Target specific options are below "Compile"
#  You can also override options from the command line.
# ______________________________________________________________________________
#
# Platform, one of Windows_NT, Linux, Web. Defaults to your OS.
_ TARGET $(uname)

# Executable name, extension is added depending on target platform.
_ NAME "png2h"

# Files to compile.
_  SRC "src/png2h.c"

# Compiler flags.
_ FLAGS ""

_ RELEASEFLAGS ""
_ DEBUGFLAGS "-O0 -g -Wall -Wextra -Wpedantic"

# ______________________________________________________________________________
#
#  Compile
# ______________________________________________________________________________
#
TYPEFLAGS=$RELEASEFLAGS
[[ "$DEBUG" != "" ]] && TYPEFLAGS=$DEBUGFLAGS

case `uname` in
	"Windows_NT")
		_ ARCH "x86_64"
		_ CC "$ARCH-w64-mingw32-gcc"
		_ EXT ".exe"
		_ PLATFORM "PLATFORM_DESKTOP"
		_ TARGETFLAGS "-lopengl32 -lgdi32 -lwinmm -Wl,--subsystem,windows"
		;;

	"Linux")
		_ CC "gcc"
		_ PLATFORM "PLATFORM_DESKTOP"
		_ TARGETFLAGS "-lGL -lm -lpthread -ldl -lrt -lX11"
		;;

	*)
		echo "Unsupported platform `uname`"
		exit 1
		;;
esac

$CC $SRC -Iinclude -Llib/`uname` -o $NAME$EXT \
	-lraylib -D$PLATFORM $FLAGS $TYPEFLAGS $TARGETFLAGS

./$NAME$EXT assets/splash.png assets/splash_web.png assets/tileset.png assets/icon.png assets/font.png