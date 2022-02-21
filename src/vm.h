#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "raylib.h"

#define u8 uint8_t
#define u16 uint16_t

#define ENTRY 0x0000
#define CLEAR_R 0x0002
#define CLEAR_G 0x0003
#define CLEAR_B 0x0004
#define SRAM_TOGGLE 0x0005

#define SRAM 0xF000
#define RESERVED 0xFF00
#define MOUSEX 0xFF00
#define MOUSEY 0xFF01
#define MOUSEL 0xFF02
#define MOUSER 0xFF03
#define RAND 0xFF04

#define RESH 30
#define REMAINDER 31

#define SCREENW 128
#define SCREENH 128

#define DBGLOG(...) if (vm->debug) printf(__VA_ARGS__);

struct VM {
	u8 mem[0x10000];
	u8 reg[32]; // [30] is result high byte, [31] is division remainder
	            // in asm you can use %h and %r
	u16 callstack[0x100];
	u16 pc;
	u8 sp;

	u8 drawX; // for DW instruction
	u8 drawY;
	u8 drawwidth;
	u8 drawheight;
	bool needdraw;

	char filename[256];
	bool debug;
	bool nosave;
	RenderTexture screen;
	Texture tileset;
	u8 scale;

	Sound cursound;
};

enum Opcode {
	OP_NOP, OP_SET, OP_LD, OP_LDI, OP_ST, OP_STI,
	OP_ADD, OP_SUB, OP_MUL, OP_DIV,
	OP_AND, OP_OR, OP_XOR,
	OP_EQ, OP_LT, OP_GT,
	OP_JMP, OP_CJ, OP_JS, OP_CJS, OP_RET,
	OP_DW, OP_AT, OP_KEY, OP_SND, OP_END,
	OP_COUNT
};

void _step(struct VM *vm);

#define get16(i) vm->mem[i] << 8 | vm->mem[i + 1]
#define consume() vm->mem[vm->pc++]
#define consume16() consume() << 8 | consume()
#define step() _step(vm)

#endif // vm.h