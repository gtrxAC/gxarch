![gxarch](assets/logo.png)

# Features
* [64K shared RAM/ROM, 4K save file](https://github.com/gtrxAC/gxarch/wiki/Memory-Map)
* 32 registers
* [24 instructions](https://github.com/gtrxAC/gxarch/wiki/Instructions)
* 128 × 128 screen, 16 user definable colors
* [4-channel audio](https://github.com/gtrxAC/gxarch/wiki/Instructions#snd-reg1-reg2-reg3-reg4-sound) powered by [rFXGen](https://github.com/raysan5/rfxgen)
* [13 example programs and counting!](https://github.com/gtrxAC/gxarch/tree/main/examples)


# Example
This example is further explained [here](https://github.com/gtrxAC/gxarch/blob/main/examples/hello.gxs).
```asm
dat main

string: dat "Hello world!"

val PRINT_CHARSPERLINE 32
val PRINT_WIDTH 4
val PRINT_HEIGHT 8
.include std/print.gxs

main:
	set %20 hi(string)
	set %21 lo(string)
	set %22 0
	set %23 0
	js print
	end
	jmp main
```


# Building

## Linux
1. Run `./setup.sh` to install raylib.
2. Run `./build.sh` to compile the project.

## Windows
1. Download [w64devkit](https://github.com/skeeto/w64devkit/releases):
* `w64devkit-x.x.x.zip` for 64-bit
* `w64devkit-i686-x.x.x.zip` for 32-bit
2. Extract w64devkit and run `w64devkit.exe`.
3. Inside w64devkit, go to the directory where you cloned gxarch.
4. Run `./setup.sh` to install raylib.
5. Run `./build.sh` to compile the project.

## Windows (cross compile)
1. Install `mingw-w64` using your package manager.
2. Run `TARGET=Windows_NT ./setup.sh` to install raylib.
3. Run `TARGET=Windows_NT ./build.sh` to compile the project.

## Assembler
1. Install [Node.js](https://nodejs.org).
2. Open a command prompt/terminal in this directory and run `node gxasm program.gxs`.
  * On some systems, it may be `nodejs` instead of `node`.
  * Replace `program.gxs` with the assembly file's name. Try it on the examples: `examples/hello.gxs`.
3. The output file is generated in the same directory as the gxs file.
4. You can specify `-r` at the end of the command to also automatically run the file. `node gxasm examples/hello.gxs -r`

## Web
1. If you're on Windows, follow the first 3 steps of the [Windows](#windows) guide.
2. Run `TARGET=Web ./setup.sh` to install raylib.
3. Run `TARGET=Web ./build.sh` to compile the project.

# Making your own programs
Documentation is still work in progress, but if you want to make your own programs, check [the wiki](https://github.com/gtrxAC/gxarch/wiki) for some resources. You can also look through the [examples](https://github.com/gtrxAC/gxarch/tree/main/examples) (some are more documented than others). If you need any help, join the [raylib Discord](https://discord.gg/raylib) and ask me (`@>gtrx<#6036`).