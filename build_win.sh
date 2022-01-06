#!/bin/sh
# gxarch build script for Windows
# Works with MinGW w64devkit: https://github.com/skeeto/w64devkit
# Use ./build_win.sh -d to compile a debug version

# Build png2h utility
cc src/png2h.c -Llib -Iinclude -lraylib -lopengl32 -lgdi32 -lwinmm -o png2h.exe

# Create headers from images
./png2h.exe assets/splash.png assets/tileset.png assets/icon.png

# Build final program
if [ "$1" = "-d" ]; then
	CFLAGS="-g"
else
	CFLAGS="-Ofast"
fi

cc src/main.c src/vm.c src/sram.c src/tinyfiledialogs.c \
	-Llib -Iinclude -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32 \
	-Wl,--subsystem,windows $CFLAGS -o gxvm.exe