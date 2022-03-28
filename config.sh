#!/bin/bash
# ______________________________________________________________________________
#
#  Build options
#  This script is sourced by build/setup, no need to run it yourself
# ______________________________________________________________________________
#
# Executable name, extension is added depending on target platform.
NAME=gxvm

# Files to compile. You can add multiple files by separating by spaces.
SRC="src/main.c src/rfxgen.c src/sram.c src/ui.c src/vm.c"

# Platform, one of Windows_NT, Linux, Web. Defaults to your OS.
# This can be set from the command line: TARGET=Web ./build.sh
[[ -z "$TARGET" ]] && TARGET=$(uname)

# Compiler flags.
# This can be set from the command line: FLAGS="-Ofast" ./build.sh
[[ -z "$FLAGS" ]] && FLAGS=""

# Compiler flags for release and debug mode
# To set debug mode, run: DEBUG=1 ./build.sh
RELEASE_FLAGS="-Os -flto -s"
DEBUG_FLAGS="-DDEBUG -O0 -g -Wall -Wextra -Wpedantic"