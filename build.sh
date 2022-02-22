#!/bin/bash
_ () { [[ "${!1}" = "" ]] && export $1="$2"; }
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
#  Build options
#  Target specific options are below "Compile"
#  You can also override options from the command line.
# ______________________________________________________________________________
#
# Platform, one of Windows_NT, Linux, Web. Defaults to your OS.
_ TARGET $(uname)

# Executable name, extension is added depending on target platform.
_ NAME "gxvm"

# Files to compile.
_  SRC "src/main.c src/sram.c src/vm.c src/ui.c src/rfxgen.c"

# Compiler flags.
_ FLAGS ""

_ RELEASEFLAGS "-Os -flto -s"
_ DEBUGFLAGS "-O0 -g -Wall -Wextra -Wpedantic"

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

# Build options for each target
case "$TARGET" in
	"Windows_NT")
		_ ARCH "x86_64"
		_ CC "$ARCH-w64-mingw32-gcc"
		_ EXT ".exe"
		_ PLATFORM "PLATFORM_DESKTOP"
		_ TARGETFLAGS "src/tinyfiledialogs.c -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32 -Wl,--subsystem,windows"
		;;

	"Linux")
		_ CC "gcc"
		_ PLATFORM "PLATFORM_DESKTOP"
		_ TARGETFLAGS "src/tinyfiledialogs.c -lGL -lm -lpthread -ldl -lrt -lX11"
		;;

	"Web")
		_ CC "emcc"
		_ EXT ".html"
		_ PLATFORM "PLATFORM_WEB"
		_ TARGETFLAGS "-s ASYNCIFY -s USE_GLFW=3 -s TOTAL_MEMORY=67108864 -s FORCE_FILESYSTEM=1 --shell-file src/shell.html -s EXPORTED_RUNTIME_METHODS=ccall -s EXPORTED_FUNCTIONS=_main,_loadfile"
		source emsdk/emsdk_env.sh
		;;

	*)
		echo "Unsupported platform $TARGET"
		exit 1
		;;
esac

$CC $SRC -Iinclude -Llib/$TARGET -o $NAME$EXT \
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