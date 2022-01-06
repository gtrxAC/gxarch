#!/bin/sh
# gxarch build script
# Use ./build.sh -d to compile a debug version

# Build png2h utility
cc src/png2h.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o png2h

# Create headers from images
./png2h assets/splash.png assets/tileset.png assets/icon.png

# Build final program
if [ "$1" = "-d" ]; then
	CFLAGS="-g"
else
	CFLAGS="-Ofast"
fi

cc src/main.c src/vm.c src/sram.c src/tinyfiledialogs.c \
	-lraylib -lGL -lm -lpthread -ldl -lrt -lX11 \
	$CFLAGS -o gxvm