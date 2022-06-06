![gxarch](assets/logo.png)
gxarch is a simple fantasy console architecture and assembly language, powered by [raylib](https://www.raylib.com).

# Features
* [32K ROM, 4K RAM, 4K save file](https://github.com/gtrxAC/gxarch/wiki/Memory-Layout)
* [64 registers](https://github.com/gtrxAC/gxarch/wiki/Registers)
* [29 instructions](https://github.com/gtrxAC/gxarch/wiki/Instructions)
* 192 × 160 screen, 16 user definable colors
* [4-channel audio](https://github.com/gtrxAC/gxarch/wiki/Syscalls#2-sys_sound-type-freq-sust-decay-play-sound) powered by [rFXGen](https://github.com/raysan5/rfxgen)
<!-- * [13 example programs and counting!](https://github.com/gtrxAC/gxarch/tree/main/examples) -->

# Example
<!-- This example is further explained [here](https://github.com/gtrxAC/gxarch/blob/main/examples/hello.gxs). -->
```asm
; Define program entry point
datl main

; Define the string we want to print
string: dat "Hello world!"

; Include print from the standard library
include "std/default_font.gxs"
include "std/print.gxs"

main:
	; Give arguments to the print function: string address and where to draw
	arg hi(string), lo(string), 0, 0

	; Call the print function
	call print

	; Finish drawing the frame
	sys SYS_END

	; End the loop
	jmp main
```


# Building
* If you don't want to build gxarch yourself, download the pre-compiled executables [here](https://nightly.link/gtrxAC/gxarch/workflows/main/main). You don't need to clone the repository unless you want the examples.
1. If you're on Windows, download [w64devkit](https://github.com/skeeto/w64devkit/releases). Make sure you get a release zip, not the source code. Extract the archive somewhere and run `w64devkit.exe`. On Linux, just open a terminal.
2. Follow the below instructions for the platform you want to build for. If you want to create your own programs, you'll also need to build the [assembler](#assembler).

## Desktop
1. Run `./setup.sh` to install raylib.
2. Run `./build.sh` to compile the project.

## Web
2. Run `TARGET=Web ./setup.sh` to install raylib.
3. Run `TARGET=Web ./build.sh` to compile the project.

## Windows (cross compile)
1. Install `mingw-w64` using your package manager.
2. Run `TARGET=Windows_NT ./setup.sh` to install raylib.
3. Run `TARGET=Windows_NT ./build.sh` to compile the project.

## Assembler
1. Run `./build_asm.sh`.
2. Run `./gxasm program.gxs` to assemble a program. On Windows, use `gxasm.exe program.gxs`.
* Replace `program.gxs` with the assembly file's name. Try it on the examples: `examples/hello.gxs`.
3. The output file is generated in the same directory as the gxs file.
4. You can specify `-r` at the end of the command to also automatically run the file. `./gxasm examples/hello.gxs -r` or `gxasm.exe examples/hello.gxs -r`

# Making your own programs
Documentation is still work in progress, but if you want to make your own programs, check [the wiki](https://github.com/gtrxAC/gxarch/wiki) for some resources.
<!-- You can also look through the [examples](https://github.com/gtrxAC/gxarch/tree/main/examples) (some are more documented than others). -->