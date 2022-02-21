#!/bin/bash
# ______________________________________________________________________________
#
#  Compile raylib project
#  Modified for gxarch
#
#  - Linux                   ./build.sh
#  - Windows (w64devkit)     ./build.sh
#  - Windows (cross compile) TARGET=Windows_NT ./build.sh
#  - Web                     TARGET=Web ./build.sh
#
#  - Debug                   DEBUG=1 ./build.sh
#  - Build and run           ./build.sh -r
# ______________________________________________________________________________
#

# Default build options, override options from the command line

# Platform, one of Windows_NT, Linux, Web. Defaults to your OS.
[[ "$TARGET" = "" ]] && TARGET=`uname`

# Executable name, extension is added depending on target platform.
[[ "$NAME" = "" ]] && NAME="gxvm"

# Compiler flags.
[[ "$FLAGS" = "" ]] && FLAGS=""

RELEASEFLAGS="-Os -flto -s"
DEBUGFLAGS="-O0 -g -Wall -Wextra -Wpedantic"

# ______________________________________________________________________________
#
#  Compile
# ______________________________________________________________________________
#
TYPEFLAGS=$RELEASEFLAGS
[[ "$DEBUG" != "" ]] && TYPEFLAGS=$DEBUGFLAGS

[[ -e lib/$TARGET ]] || ./setup.sh

# Convert images to headers
./png2h.sh

case "$TARGET" in
	"Windows_NT")
		CC="x86_64-w64-mingw32-gcc"
		EXT=".exe"
		PLATFORM="PLATFORM_DESKTOP"
		TARGETFLAGS="src/tinyfiledialogs.c -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32 -Wl,--subsystem,windows"
		;;

	"Linux")
		CC="gcc"
		PLATFORM="PLATFORM_DESKTOP"
		TARGETFLAGS="src/tinyfiledialogs.c -lGL -lm -lpthread -ldl -lrt -lX11"
		;;

	"Web")
		CC="emcc"
		EXT=".html"
		PLATFORM="PLATFORM_WEB"
		TARGETFLAGS="-s ASYNCIFY -s USE_GLFW=3 -s TOTAL_MEMORY=67108864 -s FORCE_FILESYSTEM=1 --shell-file src/shell.html -s EXPORTED_RUNTIME_METHODS=ccall -s EXPORTED_FUNCTIONS=_main,_loadfile"
		source emsdk/emsdk_env.sh
		;;

	*)
		echo "Unsupported OS $TARGET"
		exit 1
		;;
esac

$CC src/main.c src/sram.c src/vm.c src/ui.c src/rfxgen.c -Iinclude -Llib/$TARGET -o $NAME$EXT \
	-lraylib -D$PLATFORM $FLAGS $TYPEFLAGS $TARGETFLAGS

# itch.io expects html5 games to be named index.html, the names of js/data/wasm
# files can stay the same
[[ "$TARGET" = "Web" ]] && mv $NAME.html index.html

# ______________________________________________________________________________
#
#  Run if -r was specified
# ______________________________________________________________________________
#
if [[ "$1" = "-r" ]]; then
	case "$TARGET" in
		"Windows_NT") ([[ `uname` = "Linux" ]] && wine $NAME$EXT) || $NAME$EXT;;
		"Linux") ./$NAME;;
		"Web") emrun index.html;;
	esac
fi