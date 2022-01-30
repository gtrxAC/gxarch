#!/bin/sh
# gxarch build script
# Use ./build.sh -d to compile a debug version
# You can also specify your own compile flags after ./build.sh

# Build png2h utility
cc src/png2h.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o png2h

# Create headers from images
./png2h assets/splash.png assets/tileset.png assets/icon.png

# Set compile flags
# Include debug symbols if compiling a debug version, optimize for speed if not
if [ "$1" = "-d" ]; then
	CFLAGS="-g"
else
	CFLAGS="-Ofast"
fi

# Build final program
cc src/main.c src/vm.c src/sram.c src/tinyfiledialogs.c \
	-lraylib -lGL -lm -lpthread -ldl -lrt -lX11 \
	$CFLAGS $@ -o gxvm