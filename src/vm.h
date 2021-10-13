#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "raylib.h"

#define u8 uint8_t
#define u16 uint16_t

#define ENTRY 0x0000
#define SRAM 0xF000
#define RESERVED 0xFF00
#define KEY 0xFF00
#define SRAM_TOGGLE 0xFF01
#define RAND 0xFF02

#define RESH 32
#define REMAINDER 33

#define DBGLOG(...) if (vm->debug) printf(__VA_ARGS__);

struct VM {
	u8 mem[0x10000];
	u8 reg[34]; // [32] is result high byte, [33] is division remainder
	            // in asm you can use %h and %r
	u16 callstack[0x100];
	u16 pc;
	u8 sp;

	u8 drawX; // for DW instruction
	u8 drawY;
	u8 drawsize;
	bool needdraw;

	char filename[256];
	bool debug;
	bool nosave;
	RenderTexture screen;
	Texture tileset;
};

enum Instruction {
	I_NOP, I_SET, I_LD, I_LDI, I_ST, I_STI,
	I_ADD, I_SUB, I_MUL, I_DIV,
	I_AND, I_OR, I_XOR,
	I_EQ, I_LT, I_GT,
	I_JMP, I_CJ, I_JS, I_CJS, I_RET,
	I_DW, I_AT, I_KEY, I_SND, I_END,
	I_COUNT
};

void _step(struct VM *vm);

#define get16(i) vm->mem[i] << 8 | vm->mem[i + 1]
#define consume() vm->mem[vm->pc++]
#define consume16() consume() << 8 | consume()
#define step() _step(vm)

#endif