#include "vm.h"
#include "sram.h"
void err(const char *fmt, ...); // main.c

// Opcode names, used for debugging.
const char *opnames[] = {
	"nop", "set", "ld ", "ldi", "st ", "sti",
	"add", "sub", "mul", "div",
	"and", "or ", "xor",
	"eq ", "lt ", "gt ",
	"jmp", "cj ", "js ", "cjs", "ret",
	"dw ", "at ", "key", "snd", "end"
};

// gxvm to raylib key mappings
// raylib keycodes for some function keys go beyond 255 but gxarch only uses one
// byte, so they are remapped
// raylib doesn't have separate keycodes for upper/lowercase or most special
// characters, but gxarch does, so we also need to check if shift is down using
// the needshift array
const u16 gx2rl[0x100] = {
	0, 0, 0, 0, 0, 0, 0, 0, // 0x00
	KEY_BACKSPACE, KEY_TAB, KEY_ENTER, 0, 0, 0, 0, 0, // 0x08
	0, KEY_LEFT_CONTROL, KEY_LEFT_SUPER, KEY_LEFT_ALT, 0, 0, 0, 0, // 0x10
	0, 0, 0, KEY_ESCAPE, 0, 0, 0, 0, // 0x18
	' ', '1', '\'', '3', '4', '5', '7', '\'', // 0x20
	'9', '0', '8', '=', ',', '-', '.', '/', // 0x28
	'0', '1', '2', '3', '4', '5', '6', '7', // 0x30
	'8', '9', ';', ';', ',', '=', '.', '/', // 0x38
	'2', 'A', 'B', 'C', 'D', 'E', 'F', 'G', // 0x40
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 0x48
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', // 0x50
	'X', 'Y', 'Z', '[', '\\', ']', '6', '-', // 0x58
	'`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', // 0x60
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 0x68
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', // 0x70
	'X', 'Y', 'Z', '[', '\\', ']', '`', 0, // 0x78
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, // 0x80
	KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, // 0x88
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x90
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xA0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xB0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xC0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xD0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xE0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xF0
};

const bool needshift[0x100] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10
	0, 1, 1, 1, 1, 1, 1, 0, // 0x20
	1, 1, 1, 1, 0, 0, 0, 0, // 0x28
	0, 0, 0, 0, 0, 0, 0, 0, // 0x30
	0, 0, 1, 0, 1, 0, 1, 1, // 0x38
	1, 1, 1, 1, 1, 1, 1, 1, // 0x40
	1, 1, 1, 1, 1, 1, 1, 1, // 0x48
	1, 1, 1, 1, 1, 1, 1, 1, // 0x50
	1, 1, 1, 0, 0, 0, 1, 1, // 0x58
	0, 0, 0, 0, 0, 0, 0, 0, // 0x60
	0, 0, 0, 0, 0, 0, 0, 0, // 0x68
	0, 0, 0, 0, 0, 0, 0, 0, // 0x70
	0, 0, 0, 1, 1, 1, 1, 0, // 0x78
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x80
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x90
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xA0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xB0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xC0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xD0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xE0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xF0
};


void _step(struct VM *vm) {
	DBGLOG("%5d | 0x%.4X  ", vm->pc, vm->pc);

	if (vm->pc < 2 || vm->pc >= RESERVED)
		err("Attempted to execute code at 0x%.4X", vm->pc);

	u8 op = consume();
	DBGLOG("%s ", opnames[op]);

	if (op >= OP_COUNT)
		err("Invalid opcode at 0x%.4X: %d", vm->pc - 1, op);

	switch (op) {
		case OP_NOP: break;

		case OP_SET: {
			u8 reg = consume();
			vm->reg[reg] = consume();
			break;
		}

		case OP_LD: {
			u8 reg = consume();
			u16 addr = consume16();
			vm->reg[reg] = vm->mem[addr];
			if (addr == RAND) vm->mem[RAND] = GetRandomValue(0, 0xFF);
			break;
		}

		case OP_LDI: {
			u8 dest = consume();
			u8 src = consume();
			u16 addr = vm->reg[src] << 8 | vm->reg[src + 1];
			vm->reg[dest] = vm->mem[addr];
			break;
		}

		case OP_ST: {
			u8 val = vm->reg[consume()];
			u16 addr = consume16();
			vm->mem[addr] = val;
			if (addr == SRAM_TOGGLE && val) load();
			break;
		}

		case OP_STI: {
			u8 val = vm->reg[consume()];
			u8 reg = consume();
			u16 addr = vm->reg[reg] << 8 | vm->reg[reg + 1];
			vm->mem[addr] = val;
			if (addr == SRAM_TOGGLE && val) load();
			break;
		}

		#define BINOP16(op, sign) \
			case OP_ ## op: { \
				u16 result = vm->reg[consume()] sign vm->reg[consume()]; \
				vm->reg[consume()] = result & 0xFF; \
				vm->reg[RESH] = (result & 0xFF00) >> 8; \
				break; \
			}

		BINOP16(ADD, +)
		BINOP16(SUB, -)
		BINOP16(MUL, *)
		
		case OP_DIV: { \
			u8 first = vm->reg[consume()];
			u8 second = vm->reg[consume()];
			if (!second) err("Division by zero at 0x%.4X", vm->pc - 3);

			vm->reg[consume()] = first / second;
			vm->reg[REMAINDER] = first % second;
			break;
		}
		
		#define BINOP(op, sign) \
			case OP_ ## op: { \
				u8 result = vm->reg[consume()] sign vm->reg[consume()]; \
				vm->reg[consume()] = result; \
				break; \
			}

		BINOP(AND, &)
		BINOP(OR, |)
		BINOP(XOR, ^)
		BINOP(EQ, ==)
		BINOP(LT, <)
		BINOP(GT, >)

		case OP_JMP:
			vm->pc = consume16();
			break;

		case OP_CJ: {
			u8 cond = vm->reg[consume()];
			u16 addr = consume16();
			if (cond) vm->pc = addr;
			break;
		}

		case OP_JS: {
			u16 addr = consume16();
			vm->callstack[vm->sp++] = vm->pc;
			vm->pc = addr;
			break;
		}

		case OP_CJS: {
			u8 cond = vm->reg[consume()];
			u16 addr = consume16();
			if (cond) {
				vm->callstack[vm->sp++] = vm->pc;
				vm->pc = addr;
			}
			break;
		}

		case OP_RET:
			vm->pc = vm->callstack[--vm->sp];
			break;

		case OP_DW:
			vm->drawX = vm->reg[consume()];
			vm->drawY = vm->reg[consume()];
			vm->drawsize = vm->reg[consume()];
			break;
			
		case OP_AT:
			DrawTextureRec(
				vm->tileset,
				(Rectangle){vm->drawX, vm->drawY, vm->drawsize, vm->drawsize},
				(Vector2){vm->reg[consume()], vm->reg[consume()]}, WHITE
			);
			break;

		case OP_KEY: {
			u8 key = vm->reg[consume()];
			bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
			vm->reg[consume()] = shift == needshift[key] && IsKeyDown(gx2rl[key]);
			break;
		}
		
		case OP_SND:
			err("SND not implemented, used at 0x%.4X", vm->pc - 1);
			break;

		case OP_END:
			vm->needdraw = true;
			vm->mem[MOUSEX] = GetMouseX() / vm->scale;
			vm->mem[MOUSEY] = GetMouseY() / vm->scale;
			vm->mem[MOUSEL] = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
			vm->mem[MOUSER] = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
			break;
	}

	// Print all registers in debug mode
	DBGLOG("\nREG: ");
	for (int i = 0; i < 32; i++) {
		DBGLOG("%.2X ", vm->reg[i]);
	}
	DBGLOG("  RESH: %.2X  REM: %.2X\n", vm->reg[RESH], vm->reg[REMAINDER]);
}