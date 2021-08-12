# gxarch

Simple fantasy computer architecture. This repository contains `gxVM`, an emulator written in C using raylib, and `gxasm`, an assembler written in Node.js.

## Features

* 320 × 200 16-color tilemap screen
* Unlimited instructions/second, the `end` instruction draws a frame (60 fps).
* 32k RAM/ROM
* 21 instructions

## Building

### gxVM

1. Check the "Working on" page for your platform on the [raylib wiki](https://github.com/raysan5/raylib/wiki) for instructions building raylib.

2. For Linux, there is a `build.sh` file which does all the following steps for you.

3. gxVM uses a splash and tileset image built into the executable. The `png2h` utility creates header files from the `splash.png` and `tileset.png` images, which are included into `gxvm.c`.

```sh
# Build png2h utility
cc png2h.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o png2h
```

4. Use `png2h` to create the image headers.

```sh
# Create headers from images
./png2h splash.png tileset.png
```

5. Build the main file.

```sh
# Build final program
cc gxvm.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o gxvm
```

### gxasm

1. Install [Node.js](https://nodejs.org).

2. Run `npm i ohm-js` in this directory. Ohm is used for parsing the assembly files.

3. Run `node gxasm.js <filename>` to assemble a file. You can try it on files in the `examples` folder. The output file is `output.gxa`.


## Memory Layout

### `0x0000 - 0x7FFF` 32k RAM/ROM
 * ROM is loaded at `0x0000`.
 
### `0x8000 - 0x8100` call stack

### `0xB000 - 0xEFFF` tileset
 * 4096 bytes from left to right, top to bottom, creating a 128 × 128 × 4bpp image where each 8 × 8 region is a tile.

### `0xF000 - 0xF3E8` screen memory
 * 1000 bytes from left to right, top to bottom, creating a 320 × 200 image of 40 × 25 tiles, 8 × 8 pixels each.

### `0xFFF0 - 0xFFFF` reserved
 * `0xFFFx` registers 0-9
 * `0xFFFA` result register "R"
 * `0xFFFB` random number
 * `0xFFFC` program counter
 * `0xFFFE` stack pointer
 
Unused memory areas can be defined by implementations.


## ROM Layout

* The assembly files have the `.gxs` extension and must be assembled with `node gxasm.js filename.gxs` before running. The assembler outputs a `output.gxa` file.
* ROMs consist of two files: a binary file with the `.gxa` extension containing code/data, and a tileset `.png` image.
* They should have the same filename without extension, for example the tileset for `mygame.gxa` is `mygame.png`.
* If a matching image file is not found, the default `tileset.png` is used.
* The ROM is loaded at the beginning of memory. It should be 32k or smaller to fit into ROM/RAM. The first two bytes of the file indicate the program entry point.
* The tileset contains a 128 × 128 × 4bpp indexed palette image. The colors in the palette don't matter. It is loaded at `0xB000`. If this file doesn't exist, a default built-in tileset is used.

 
## Instructions

* The first byte of an instruction indicates the opcode, and if any arguments are pointers.

```
0 0 000000   ... 0-4 bytes for arguments
| |   |
| |   '- opcode
| '- is 2nd argument a pointer?
'- is 1st argument a pointer?
```

* If an argument is a pointer, two bytes are read for that argument instead of one, and the byte at that address is the argument's value.

### Instruction Set

```
val = 1 byte argument, 2 bytes if pointer flag set
adr = always 2 byte argument

* = modifies the R register 0xFFFA

hex dec bin    op  arg arg   description
00  00  000000 nop --- ---   Do nothing
01  01  000001 set adr val   Set adr to val
02  02  000010 mov adr adr   Copy value at adrA to adrB

03  03  000011 add val val * Add two numbers
04  04  000100 sub val val * Subtract two numbers
05  05  000101 mul val val * Multiply two numbers
06  06  000110 div val val * Divide two numbers

07  07  000111 and val val * Bitwise AND
08  08  001000 or  val val * Bitwise OR
09  09  001001 xor val val * Bitwise exclusive OR
0A  10  001010 not val --- * Bitwise NOT

0B  11  001011 equ val val * Return 1 if valA = valB, else 0
0C  12  001100 lt  val val * Return 1 if valA < valB
0D  13  001101 gt  val val * Return 1 if valA > valB

0E  14  001110 jmp adr ---   Jump to adr
0F  15  001111 cj  adr adr   Jump to adrA if value at adrB != 0
10  16  010000 js  adr ---   Jump to subroutine adr
11  17  010001 cjs adr adr   Jump to subroutine adrA if value at adrB != 0
12  18  010010 ret --- ---   Return from subroutine

13  19  010011 key val ---   Return how many frames key val has been pressed for, 0 = not pressed
14  20  010100 end --- ---   Draw current frame
```


## Implementation Notes

* Tilesets are currently not loaded into `0xB000` like in the spec, so tiles cannot be modified at runtime.
* The amount of colors in tileset images is currently not checked, but you should still only use 16 colors.