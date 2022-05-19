#ifndef VM_H
#define VM_H

#include "raylib.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// #include <ctype.h>

#define u8 uint8_t
#define u16 uint16_t

#define SCREENW 192
#define SCREENH 160

typedef enum Opcode {
	OP_NOP, OP_SET, OP_LD, OP_ST,
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
	OP_AND, OP_OR, OP_XOR,
	OP_EQ, OP_LT, OP_GT,
	OP_EQJ, OP_LTJ, OP_GTJ,
	OP_EQC, OP_LTC, OP_GTC,
	OP_ARG, OP_JMP, OP_CJ, OP_CALL, OP_CC, OP_RET, OP_RETV,
	OP_SYS,
	OP_COUNT
} Opcode;

typedef enum Syscall {
	SYS_DRAW, SYS_END, SYS_SOUND,
	SYS_COUNT
} Syscall;

typedef enum State {
	ST_IDLE,
	ST_RUNNING,
	ST_PAUSED
} State;

typedef struct Registers {
	union {
		struct {
			u8 gp[32];
			u8 args[8];
			u8 local[8];
			u8 rVal;
			u8 mouseX;
			u8 mouseY;
			u8 mouseL;
			u8 mouseR;
			u8 up;
			u8 down;
			u8 left;
			u8 right;
			u8 act[3];
			u8 clearX;
			u8 clearY;
			u8 rand;
			u8 resH;
		};
		u8 data[64];
	};
} Registers;

typedef struct VM {
	Registers reg;

	union {
		struct {
			u8 rom[0x8000];
			u8 unused[0x6000];
			u8 ram[0x1000];
			u8 sram[0x1000];
		};
		u8 mem[0x10000];
	};

	u16 pc;
	u8 sp;
	u8 argsp;

	u16 callStack[256];
	u8 argStack[256][8];
	u8 localStack[256][8];

	State state;

	bool needDraw;
	int scale;
	Texture tileset;
	RenderTexture screen;
	Sound curSound[4];

	bool debug;
	bool noSave;
	char fileName[256];
} VM;

void step(VM *vm);
#define get16(memType, i) vm->memType[i] << 8 | vm->memType[i + 1]
#define consume() vm->rom[vm->pc++]
#define consume16() consume() << 8 | consume()

#endif // vm.h