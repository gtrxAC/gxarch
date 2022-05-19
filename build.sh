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
source config.sh

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
		TARGET_FLAGS="src/tinyfiledialogs.c -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32 -Wl,--subsystem,windows"
		;;

	"Linux")
		CC="gcc"
		PLATFORM="PLATFORM_DESKTOP"
		TARGET_FLAGS="src/tinyfiledialogs.c -lm -ldl -lpthread"
		;;

	"Web")
		CC="emcc"
		EXT=".html"
		PLATFORM="PLATFORM_WEB"
		TARGET_FLAGS="-s ASYNCIFY -s USE_GLFW=3 -s TOTAL_MEMORY=67108864 -s FORCE_FILESYSTEM=1 --shell-file src/shell.html -s EXPORTED_RUNTIME_METHODS=ccall -s EXPORTED_FUNCTIONS=_main,_loadFile"
		source emsdk/emsdk_env.sh
		;;

	*)
		echo "Unsupported platform $TARGET"
		exit 1
		;;
esac

# Stop the build process if anything fails
set -e

# Convert images to headers to be embedded into the executable
cd assets
xxd -i font.png > font.h
xxd -i icon.png > icon.h
xxd -i tileset.png > tileset.h
cd ..

# Compile assembler if needed
[[ -e gxasm ]] || ./build_asm.sh

# Assemble intro animation and embed into the executable
if [[ "$TARGET" = "Web" ]]; then
	./gxasm assets/intro_web.gxs
	xxd -i assets/intro_web.gxa > assets/intro.h
else
	./gxasm assets/intro.gxs
	xxd -i assets/intro.gxa > assets/intro.h
fi

$CC $SRC -Iinclude -Llib/$TARGET -o $NAME$EXT \
	-lraylib -D$PLATFORM $FLAGS $TARGET_FLAGS

# itch.io expects html5 games to be named index.html, js/data/wasm filenames can
# stay the same
[[ "$TARGET" = "Web" ]] && mv $NAME.html index.html

# ______________________________________________________________________________
#
#  Run if -r was specified
# ______________________________________________________________________________
#
if [[ "$1" = "-r" ]]; then
	case "$TARGET" in
		"Windows_NT") ([[ $(uname) = "Linux" ]] && wine $NAME$EXT) || $NAME$EXT;;
		"Linux") ./$NAME;;
		"Web") emrun index.html;;
	esac
fi