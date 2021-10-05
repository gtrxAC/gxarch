#include "vm.h"
void err(const char *fmt, ...); // main.c

// Opcode names, used for debugging.
const u8 *instnames[] = {
	"nop", "set", "mov",
	"add", "sub", "mul", "div",
	"and", "or ", "xor", "not",
	"equ", "lt ", "gt ",
	"jmp", "cj ", "js ", "cjs", "ret",
	"key", "end"
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

// Converts an address/value into a string. Registers are converted into their
// names, such as r0 and res, for easier debugging.
const u8 *_addr2str(struct VM *vm, u16 addr, u8 digits) {
	if (addr >= REG(0) && addr <= REG(15)) {
		sprintf(vm->addr2str_buf, "r%X", addr & 0x0F);
		return vm->addr2str_buf;
	}

	switch (addr) {
		case RESULT: return "resh"; break;
		case RESULT + 1: return "res"; break;
		case RAND: return "rand"; break;

		default:
			sprintf(vm->addr2str_buf, "0x%.*x", digits, addr);
			return vm->addr2str_buf;
	}
}

// Set a 16-bit value.
void _set16(struct VM *vm, u16 index, u16 val) {
	vm->mem[index] = (val & 0xFF00) >> 8;
	vm->mem[index + 1] = val & 0xFF;
}

// Get the next instruction byte.
u8 _consume(struct VM *vm) {
	u8 res = vm->mem[get16(PC)];
	set16(PC, get16(PC) + 1);
	return res;
}

// Get a value argument.
u8 _getval(struct VM *vm, u8 ptr) {
	u8 result;
	if (ptr) {
		u16 addr = consume16();
		result = vm->mem[addr];
		DBGLOG("*%s (= %s) ", addr2str(addr, 4), addr2str(result, 2));
	} else {
		result = consume();
		DBGLOG("%s ", addr2str(result, 2));
	}
	return result;
}

// Get an address argument.
u16 _getaddr(struct VM *vm, u8 ptr) {
	u16 result;
	if (ptr) {
		u16 addr = consume16();
		result = get16(addr);
		DBGLOG("*%s (= %s) ", addr2str(addr, 4), addr2str(result, 4));
	} else {
		result = consume16();
		DBGLOG("%s ", addr2str(result, 4));
	}
	return result;
}

// Run one instruction.
void _step(struct VM *vm) {
	// Update random number register.
	vm->mem[RAND] = GetRandomValue(0, 0xFF);

	if (get16(PC) < 2 || get16(PC) > REG(0))
		err("Attempted to execute code at 0x%.4x", get16(PC));

	u8 inst = consume();
	u8 arg1ptr = inst & 0b10000000;
	u8 arg2ptr = inst & 0b01000000;
	inst &= 0b00111111;

	if (inst > I_COUNT) {
		err("Invalid opcode at 0x%.4x: %d", get16(PC) - 1, inst);
		return;
	}

	DBGLOG("%5d | 0x%.4x  ", get16(PC) - 1, get16(PC) - 1);
	DBGLOG("%s ", instnames[inst]);

	switch (inst) {
		case I_NOP: break;

		case I_SET:
			vm->mem[getaddr(arg1ptr)] = getval(arg2ptr);
			break;

		case I_MOV: {
			u16 src = getaddr(arg1ptr);
			u16 dest = getaddr(arg2ptr);
			vm->mem[dest] = vm->mem[src];
			break;
		}

		#define BINOP(name, op) \
			case I_ ## name:\
				set16(RESULT, getval(arg1ptr) op getval(arg2ptr));\
				break;

		BINOP(ADD, +)
		BINOP(SUB, -)
		BINOP(MUL, *)
		BINOP(DIV, /)
		BINOP(AND, &)
		BINOP(OR, |)
		BINOP(XOR, ^)

		case I_NOT:
			vm->mem[RESULT + 1] = ~getval(arg1ptr);
			break;

		BINOP(EQU, ==)
		BINOP(LT, <)
		BINOP(GT, >)

		case I_JMP:
			set16(PC, getaddr(arg1ptr));
			break;

		case I_CJ: {
			u8 cond = vm->mem[getaddr(arg1ptr)];
			u16 addr = getaddr(arg2ptr);
			if (cond) set16(PC, addr);
			break;
		}

		case I_JS:
			set16(get16(SP), get16(PC) + 1);
			set16(SP, get16(SP) + 2);
			set16(PC, getaddr(arg1ptr));
			break;

		case I_CJS: {
			u8 cond = vm->mem[getaddr(arg1ptr)];
			u16 addr = getaddr(arg2ptr);
			
			if (cond) {
				set16(get16(SP), get16(PC));
				set16(SP, get16(SP) + 2);
				set16(PC, addr);
			}
			break;
		}

		case I_RET:
			set16(get16(SP), 0);
			set16(SP, get16(SP) - 2);
			set16(PC, get16(get16(SP)) + 1);

			if (get16(SP) < STACK)
				err("Call stack underflow, 0x%.4x < 0x%.4x", get16(SP), STACK);

			DBGLOG("-> 0x%.4x", get16(PC));
			break;

		case I_END: vm->needdraw = true; break;

		case I_KEY:
			vm->mem[RESULT + 1] = IsKeyDown(keymap[getval(arg1ptr)]);
			DBGLOG("'%c'", vm->mem[RESULT + 1]);
			break;
	}

	DBGLOG("\n");
}