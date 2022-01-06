![gxarch](assets/logo.png)

# Features
* [64K shared RAM/ROM, 4K save file](https://github.com/gtrxAC/gxarch/wiki/Memory-Map)
* 32 registers
* [24 instructions](https://github.com/gtrxAC/gxarch/wiki/Instructions)
* 128 × 128 screen, 16 user definable colors
* [11 example programs and counting!](https://github.com/gtrxAC/gxarch/tree/main/examples)


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
1. Clone gxarch.
2. Build raylib following [this tutorial](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux).
3. Run `build.sh`. Use `build.sh -d` if you want to build a debug version.

## Windows
1. Clone gxarch.
2. Download [w64devkit](https://github.com/skeeto/w64devkit/releases):
* `w64devkit-x.x.x.zip` for 64-bit
* `w64devkit-i686-x.x.x.zip` for 32-bit
3. Download [raylib](https://github.com/raysan5/raylib/releases):
* `raylib-x.x.x_win64_mingw-w64.zip` for 64-bit
* `raylib-x.x.x_win32_mingw-w64.zip` for 32-bit
4. Extract the `include` and `lib` folders from raylib into this directory.
5. Extract w64devkit and run `w64devkit.exe`.
6. Inside w64devkit, go to the directory where you cloned gxarch and run `build_win.sh`. Use `build.sh -d` if you want to build a debug version.

## Assembler
1. Install [Node.js](https://nodejs.org).
2. Open a command prompt/terminal in this directory and run `node gxasm program.gxs`.
  * On some systems, it may be `nodejs` instead of `node`.
  * Replace `program.gxs` with the assembly file's name. Try it on the examples: `examples/hello.gxs`.
3. The output file is generated in the same directory as the gxs file.
4. You can specify `-r` at the end of the command to also automatically run the file. `node gxasm examples/hello.gxs -r`

## Web
`build_web.js` can generate HTML files out of gxarch programs, using `gxvm.html` as a template. The HTML file contains everything needed to run the program, [except the tileset](https://github.com/gtrxAC/gxarch/wiki/Tilesets#default-tileset-in-gxvmhtml) (and audio files in the future).
1. Install [Node.js](https://nodejs.org). (If you use the assembler you should already have this)
2. Open a command prompt/terminal and run `node build_web program.gxa`.
  * Replace `program.gxa` with your program's name. Try it on the examples after [assembling](#assembler) them: `examples/hello.gxa`.
3. The output file is generated in the same directory as the gxa file.

# Making your own programs
Documentation is still work in progress, but if you want to make your own programs, check [the wiki](https://github.com/gtrxAC/gxarch/wiki) for some resources. You can also look through the [examples](https://github.com/gtrxAC/gxarch/tree/main/examples) (some are more documented than others). If you need any help, join the [raylib Discord](https://discord.gg/raylib) and ask me (`@>gtrx<#6036`).