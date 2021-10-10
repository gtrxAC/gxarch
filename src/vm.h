#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "raylib.h"

#define u8 uint8_t
#define u16 uint16_t

#define ENTRY 0x0000
#define SRAM 0xE000
#define VRAM 0xF000
#define RAND 0xFFFB

#define DBGLOG(...) if (vm->debug) printf(__VA_ARGS__);

struct VM {
	u8 mem[0x10000];
	u8 reg[32];
	u8 resh;
	u16 callstack[0x100];
	u16 pc;
	u8 sp;

	u8 drawX; // for DW instruction
	u8 drawY;
	u8 drawsize;
	bool needdraw;

	bool debug;
	RenderTexture screen;
	Texture tileset;
	// u8 addr2str_buf[8];
};

enum Instruction {
	I_NOP, I_SET, I_LD, I_ST,
	I_ADD, I_SUB, I_MUL, I_DIV,
	I_AND, I_OR, I_XOR,
	I_EQ, I_LT, I_GT,
	I_JMP, I_CJ, I_JS, I_CJS, I_RET,
	I_DW, I_AT, I_KEY, I_SND, I_END,
	I_COUNT
};

// const u8 *_addr2str(struct VM *vm, u16 addr, u8 digits);
// void _set16(struct VM *vm, u16 index, u16 val);
// u8 _consume(struct VM *vm);
// u8 _getval(struct VM *vm, u8 ptr);
// u16 _getaddr(struct VM *vm, u8 ptr);
void _step(struct VM *vm);

// #define addr2str(a, d) _addr2str(vm, a, d)
// #define set16(i, v) _set16(vm, i, v)
#define get16(i) vm->mem[i] << 8 | vm->mem[i + 1]
#define consume() vm->mem[vm->pc++]
#define consume16() consume() << 8 | consume()
// #define getval(p) _getval(vm, p)
// #define getaddr(p) _getaddr(vm, p)
#define step() _step(vm)