#include "vm.h"
#include "sram.h"
void err(const char *fmt, ...); // main.c

// Opcode names, used for debugging.
const u8 *instnames[] = {
	"nop", "set", "ld ", "st ",
	"add", "sub", "mul", "div",
	"and", "or ", "xor",
	"equ", "lt ", "gt ",
	"jmp", "cj ", "js ", "cjs", "ret",
	"dw ", "at ", "key", "snd", "end"
};

// gxvm to raylib key mappings
const u16 keymap[0x100] = {
	0, 0, 0, 0, 0, 0, 0, 0, // 0x07
	0, KEY_BACKSPACE, KEY_TAB, KEY_ENTER, 0, 0, 0, 0, // 0x0F
	0, KEY_LEFT_CONTROL, KEY_LEFT_SUPER, KEY_LEFT_ALT, 0, 0, 0, 0, // 0x17
	0, 0, 0, KEY_ESCAPE, 0, 0, 0, 0, // 0x1F
	' ', 0, 0, 0, 0, 0, 0, '\'',
	0, 0, 0, 0, ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 0, ';', 0, '=', 0, 0,
	0, 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', 0, 0,
	'`', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x6F
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x7F
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, // 0x87
	KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, // 0x8F
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x9F
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xAF
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xBF
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xCF
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xDF
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xEF
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xFF
};

void _step(struct VM *vm) {
	DBGLOG("%5d | 0x%.4X  ", vm->pc, vm->pc);

	vm->mem[RAND] = GetRandomValue(0, 0xFF);
	vm->mem[KEY] = GetKeyPressed();

	if (vm->pc < 2 || vm->pc >= RESERVED)
		err("Attempted to execute code at 0x%.4X", vm->pc);

	u8 inst = consume();
	DBGLOG("%s ", instnames[inst]);

	if (inst >= I_COUNT)
		err("Invalid opcode at 0x%.4X: %d", vm->pc - 1, inst);

	switch (inst) {
		case I_NOP: break;

		case I_SET: {
			// A one liner like vm->reg[consume()] = consume(); doesn't work for
			// me, the consume()s are done in the opposite order??
			u8 reg = consume();
			vm->reg[reg] = consume();
			break;
		}

		case I_LD:
			vm->reg[consume()] = vm->mem[consume16()];
			break;

		case I_ST: {
			u8 val = vm->reg[consume()];
			u16 addr = consume16();
			vm->mem[addr] = val;
			if (addr == SRAM_TOGGLE && val) load();
			break;
		}

		#define BINOP16(op, sign) \
			case I_ ## op: { \
				u16 result = vm->reg[consume()] sign vm->reg[consume()]; \
				vm->reg[consume()] = result & 0xFF; \
				vm->resh = (result & 0xFF00) >> 8; \
				break; \
			}

		BINOP16(ADD, +)
		BINOP16(SUB, -)
		BINOP16(MUL, *)
		BINOP16(DIV, /)
		
		#define BINOP(op, sign) \
			case I_ ## op: { \
				u8 result = vm->reg[consume()] sign vm->reg[consume()]; \
				vm->reg[consume()] = result; \
			}

		BINOP(AND, &)
		BINOP(OR, |)
		BINOP(XOR, ^)
		BINOP(EQ, ==)
		BINOP(LT, <)
		BINOP(GT, >)

		case I_JMP:
			vm->pc = consume16();
			break;

		case I_CJ:
			if (vm->reg[consume()]) vm->pc = consume16();
			break;

		case I_JS:
			vm->callstack[vm->sp++] = vm->pc;
			vm->pc = consume16();
			break;

		case I_CJS: {
			u8 cond = vm->reg[consume()];
			u16 addr = consume16();
			if (cond) {
				vm->callstack[vm->sp++] = vm->pc;
				vm->pc = addr;
			}
			break;
		}

		case I_RET:
			vm->pc = vm->callstack[--vm->sp];
			break;

		case I_DW:
			vm->drawX = vm->reg[consume()];
			vm->drawY = vm->reg[consume()];
			vm->drawsize = vm->reg[consume()];
			break;
			
		case I_AT:	
			if (!vm->drawsize) err("Draw size is 0 or DW not used before AT at 0x%.4X", vm->pc - 1);
			
			DrawTextureRec(
				vm->tileset,
				(Rectangle){vm->drawX, vm->drawY, vm->drawsize, vm->drawsize},
				(Vector2){vm->reg[consume()], vm->reg[consume()]}, WHITE
			);

			vm->drawX = vm->drawY = vm->drawsize = 0;
			break;

		case I_KEY: {
			u8 key = vm->reg[consume()];
			vm->reg[consume()] = IsKeyDown(keymap[key]);
			break;
		}
		
		case I_SND:
			err("SND not implemented, used at 0x%.4X", vm->pc - 1);
			break;

		case I_END:
			vm->needdraw = true;
			break;
	}

	DBGLOG("\nREG: "); for (int i = 0; i < 32; i++) DBGLOG("%.2X " , vm->reg[i]); DBGLOG("  RESH: %.2X", vm->resh);
	DBGLOG("\n");
}