#!/bin/sh

# Build png2h utility
cc png2h.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o png2h

# Create headers from images
./png2h splash.png tileset.png

# Build final program
cc gxvm.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -g -o gxvm