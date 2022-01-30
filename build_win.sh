#!/bin/sh
# gxarch build script for Windows
# Works with MinGW w64devkit: https://github.com/skeeto/w64devkit
# Use ./build_win.sh -d to compile a debug version
# You can also specify your own compile flags after ./build_win.sh

# Build png2h utility
cc src/png2h.c -Llib -Iinclude -lraylib -lopengl32 -lgdi32 -lwinmm -o png2h.exe

# Create headers from images
./png2h.exe assets/splash.png assets/tileset.png assets/icon.png

# Set compile flags
# Include debug symbols if compiling a debug version, optimize for speed if not
if [ "$1" = "-d" ]; then
	CFLAGS="-g"
else
	CFLAGS="-Ofast"
fi

# Build final program
# -lcomdlg32 -lole32 needed for tinyfiledialogs
# -Wl,--subsystem,windows hides the console window
cc src/main.c src/vm.c src/sram.c src/tinyfiledialogs.c \
	-Llib -Iinclude -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32 \
	-Wl,--subsystem,windows $CFLAGS $@ -o gxvm.exe