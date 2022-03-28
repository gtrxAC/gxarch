#!/bin/bash
# ______________________________________________________________________________
#
#  Set up raylib project
#  Modified for gxarch
#
#  - Linux                   ./setup.sh
#  - Windows (w64devkit)     ./setup.sh
#  - Windows (cross compile) TARGET=Windows_NT ./setup.sh
#  - Web                     TARGET=Web ./setup.sh
# ______________________________________________________________________________
#
source config.sh

# Set up directory structure
mkdir --parents include src assets lib/$TARGET

# ______________________________________________________________________________
#
#  Install or update raylib
# ______________________________________________________________________________
#
if [[ -e raylib ]]; then
	if command -v git > /dev/null; then
		cd raylib
		git pull
		cd ..
	fi
else
	if command -v git > /dev/null; then
		git clone https://github.com/raysan5/raylib --depth 1
	else
		echo "raylib directory not found, download it from https://github.com/raysan5/raylib or install git"
		exit 1
	fi
fi

# ______________________________________________________________________________
#
#  Build raylib
# ______________________________________________________________________________
#
case "$TARGET" in
	"Linux")
		cd raylib/src
		make || make -e
		mv libraylib.a ../../lib/$TARGET
		cp raylib.h ../../include
		make clean || make clean -e || rm -fv *.o
		cd ../..
		;;

	"Windows_NT")
		# To build for 32-bit, set ARCH to i686 in both build and setup.sh, then
		# run setup again
		ARCH="x86_64"

		if command -v $ARCH-w64-mingw32-gcc > /dev/null; then
			cd raylib/src
			make CC=$ARCH-w64-mingw32-gcc AR=$ARCH-w64-mingw32-ar OS=Windows_NT || \
			make CC=$ARCH-w64-mingw32-gcc AR=$ARCH-w64-mingw32-ar OS=Windows_NT -e
			mv libraylib.a ../../lib/$TARGET
			cp raylib.h ../../include
			make clean || make clean -e || rm -fv *.o
			cd ../..
		else
			if [[ `uname` = "Linux" ]]; then
					echo "Please install mingw-w64 using your package manager"
					exit 1
			else
				echo "Compiler for $ARCH not found, make sure you're using https://github.com/skeeto/w64devkit/releases for the correct architecture"
				exit 1
			fi
		fi
		;;

	"Web")
		if [[ ! -e emsdk ]]; then
			if command -v git > /dev/null; then
				git clone https://github.com/emscripten-core/emsdk --depth 1
				cd emsdk
				./emsdk install latest
				./emsdk activate latest
				cd ..
			else
				echo "emsdk directory not found, see https://emscripten.org/docs/getting_started/downloads.html or install git"
				exit 1
			fi
		fi

		[[ -e src/shell.html ]] || cp raylib/src/minshell.html src/shell.html

		source emsdk/emsdk_env.sh
		cd raylib/src
		make PLATFORM=PLATFORM_WEB || make PLATFORM=PLATFORM_WEB -e
		mv libraylib.a ../../lib/$TARGET
		cp raylib.h ../../include
		make clean || make clean -e || rm -fv *.o
		cd ../..
		;;

	*)
		echo "Unsupported platform $TARGET"
		exit 1
		;;
esac