#!/bin/sh

# Build png2h utility
cc src/png2h.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o png2h

# Create headers from images
./png2h assets/splash.png assets/tileset.png assets/icon.png

# Build final program
cc src/main.c src/vm.c src/sram.c src/tinyfiledialogs.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -g -o gxvm

# Delete temporary image headers
rm splash.h tileset.h icon.h