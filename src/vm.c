#include "vm.h"
#include "sram.h"
#include "rfxgen.h"
void err(const char *fmt, ...); // main.c

// Opcode names, used for debugging.
const char *opnames[] = {
	"nop  ", "set  ", "ld   ", "st   ",
	"add  ", "sub  ", "mul  ", "div  ", "mod  ",
	"and  ", "or   ", "xor  ",
	"eq   ", "lt   ", "gt   ",
	"eqj  ", "ltj  ", "gtj  ",
	"eqc  ", "ltc  ", "gtc  ",
	"arg  ", "jmp  ", "cj   ", "call ", "cc   ", "ret  ", "retv ",
	"sys  "
};

// Syscall names, used for debugging.
const char *sysnames[] = {
	"(draw)", "(end)", "(sound)"
};

void call(VM *vm, u16 addr) {
	u8 temp[8];

	for (int i = 0; i < 8; i++) {
		temp[i] = vm->reg.args[i];
		vm->reg.args[i] = vm->argStack[vm->sp][i];
		vm->argStack[vm->sp][i] = temp[i];
		vm->localStack[vm->sp][i] = vm->reg.local[i];
		vm->reg.local[i] = 0;
	}

	vm->callStack[vm->sp++] = vm->pc;
	vm->pc = addr;
	vm->argsp = 0;
}

void step(VM *vm) {
	u16 startPC = vm->pc;
	
	if (vm->pc < 0x0005 || vm->pc > 0x7FFF) {
		err("Attempted to execute code at 0x%.4X", startPC);
		return;
	}

	u8 opByte = consume();
	u8 op = opByte & 0b00011111;

	if (op >= OP_COUNT) {
		err("Invalid opcode at 0x%.4X: %d", startPC, op);
		return;
	}

	u8 arg1Ptr = opByte & 0b10000000;
	u8 arg2Ptr = opByte & 0b01000000;
	u8 arg3Ptr = opByte & 0b00100000;

	vm->reg.rand = GetRandomValue(0, 0xFF);

	char debugLine[128] = {0};
	sprintf(debugLine, "0x%.4X  ", startPC);
	strcat(debugLine, opnames[op]);

	switch (op) {
		#define CHECKREG(r) \
			if (r > 63) { \
				err("Invalid register access (%%%d) at 0x%.4X", r, startPC); \
				return; \
			}

		#define DEREFPTR(cond, var) \
			if (cond) { \
				CHECKREG(var); \
				strcat(debugLine, TextFormat("[%.2X]->%.2X ", var, vm->reg.data[var])); \
				var = vm->reg.data[var]; \
			} else { \
				strcat(debugLine, TextFormat("%.2X ", var)); \
			}

		#define CONSUMEADDR(cond, var) \
			if (cond) { \
				u8 ptr = consume(); \
				var = get16(reg.data, ptr); \
				strcat(debugLine, TextFormat("[%.2X]->%.4X ", ptr, var)); \
			} else { \
				var = consume16(); \
				strcat(debugLine, TextFormat("%.4X ", var)); \
			}

		case OP_NOP: break;

		case OP_SET: {
			u8 reg = consume();
			DEREFPTR(arg1Ptr, reg);

			u8 val = consume();
			DEREFPTR(arg2Ptr, val);

			CHECKREG(reg);
			vm->reg.data[reg] = val;
			break;
		}

		case OP_LD: {
			u8 reg = consume();
			DEREFPTR(arg1Ptr, reg);

			u16 addr;
			CONSUMEADDR(arg2Ptr, addr);

			if (addr > 0x7FFF && addr < 0xE000) {
				err("Invalid memory read (0x%.4X) at 0x%.4X", addr, startPC);
				return;
			}

			CHECKREG(reg);
			vm->reg.data[reg] = vm->mem[addr];
			break;
		}

		case OP_ST: {
			u8 reg = consume();
			DEREFPTR(arg1Ptr, reg);

			u16 addr;
			CONSUMEADDR(arg2Ptr, addr);

			if (addr < 0xE000) {
				err("Invalid memory write (0x%.4X) at 0x%.4X", addr, startPC);
				return;
			}

			CHECKREG(reg);
			vm->mem[addr] = vm->reg.data[reg];
			break;
		}

		#define BINOP16(op, sign) \
			case OP_ ## op: { \
				u8 first = consume(); \
				DEREFPTR(arg1Ptr, first); \
				\
				u8 second = consume(); \
				DEREFPTR(arg2Ptr, second); \
				\
				u8 dest = consume(); \
				DEREFPTR(arg3Ptr, dest); \
				\
				u16 result = first sign second; \
				CHECKREG(dest); \
				vm->reg.data[dest] = result & 0xFF; \
				vm->reg.resH = (result & 0xFF00) >> 8; \
				break; \
			}

		BINOP16(ADD, +)
		BINOP16(SUB, -)
		BINOP16(MUL, *)

		case OP_DIV: {
			u8 first = consume();
			DEREFPTR(arg1Ptr, first);
		
			u8 second = consume();
			DEREFPTR(arg2Ptr, second);
			if (!second) {
				err("Division by zero at 0x%.4X", startPC);
				return;
			}
		
			u8 dest = consume();
			DEREFPTR(arg3Ptr, dest);

			CHECKREG(dest);
			vm->reg.data[dest] = first / second;
			break;
		}

		case OP_MOD: {
			u8 first = consume();
			DEREFPTR(arg1Ptr, first);
		
			u8 second = consume();
			DEREFPTR(arg2Ptr, second);
			if (!second) {
				err("Division by zero (mod) at 0x%.4X", startPC);
				return;
			}
		
			u8 dest = consume();
			DEREFPTR(arg3Ptr, dest);

			CHECKREG(dest);
			vm->reg.data[dest] = first % second;
			break;
		}

		#define BINOP(op, sign) \
			case OP_ ## op: { \
				u8 first = consume(); \
				DEREFPTR(arg1Ptr, first); \
				\
				u8 second = consume(); \
				DEREFPTR(arg2Ptr, second); \
				\
				u8 dest = consume(); \
				DEREFPTR(arg3Ptr, dest); \
				\
				CHECKREG(dest); \
				vm->reg.data[dest] = first sign second; \
				break; \
			}

		BINOP(AND, &)
		BINOP(OR, |)
		BINOP(XOR, ^)
		BINOP(EQ, ==)
		BINOP(LT, <)
		BINOP(GT, >)

		#define BINOPJ(op, sign) \
			case OP_ ## op: { \
				u8 first = consume(); \
				DEREFPTR(arg1Ptr, first); \
				\
				u8 second = consume(); \
				DEREFPTR(arg2Ptr, second); \
				\
				u8 cond = first sign second; \
				\
				u16 addr; \
				CONSUMEADDR(arg3Ptr, addr); \
				\
				if (cond) vm->pc = addr; \
				break; \
			}

		BINOPJ(EQJ, ==)
		BINOPJ(LTJ, <)
		BINOPJ(GTJ, >)

		#define BINOPCALL(op, sign) \
			case OP_ ## op: { \
				u8 first = consume(); \
				DEREFPTR(arg1Ptr, first); \
				\
				u8 second = consume(); \
				DEREFPTR(arg2Ptr, second); \
				\
				u8 cond = first sign second; \
				\
				u16 addr; \
				CONSUMEADDR(arg3Ptr, addr); \
				\
				if (cond) call(vm, addr); \
				break; \
			}

		BINOPCALL(EQC, ==)
		BINOPCALL(LTC, <)
		BINOPCALL(GTC, >)

		case OP_ARG: {
			u8 val = consume();
			DEREFPTR(arg1Ptr, val);
			
			if (vm->argsp > 7) {
				err("Argument overflow at 0x%.4X", startPC);
				return;
			}

			vm->argStack[vm->sp][vm->argsp++] = val;
			break;
		}

		case OP_JMP:
			CONSUMEADDR(arg1Ptr, vm->pc);
			break;

		case OP_CJ: {
			u8 condReg = consume();
			strcat(debugLine, TextFormat("[%.2X]->", condReg));

			CHECKREG(condReg);
			u8 cond = vm->reg.data[condReg];
			DEREFPTR(arg1Ptr, cond);

			u16 addr;
			CONSUMEADDR(arg2Ptr, addr);

			if (cond) vm->pc = addr;
			break;
		}

		case OP_CALL: {
			u16 addr;
			CONSUMEADDR(arg1Ptr, addr);
			
			call(vm, addr);
			break;
		}

		case OP_CC: {
			u8 condReg = consume();
			strcat(debugLine, TextFormat("[%.2X]->", condReg));

			CHECKREG(condReg);
			u8 cond = vm->reg.data[condReg];
			DEREFPTR(arg1Ptr, cond);

			u16 addr;
			CONSUMEADDR(arg2Ptr, addr);
			
			if (cond) call(vm, addr);
			break;
		}

		case OP_RET:
			vm->reg.rVal = 0;
			vm->pc = vm->callStack[--vm->sp];

			for (int i = 0; i < 8; i++) {
				vm->reg.args[i] = vm->argStack[vm->sp][i];
				vm->reg.local[i] = vm->localStack[vm->sp][i];
			}
			break;

		case OP_RETV: {
			u8 val = consume();
			DEREFPTR(arg1Ptr, val);
			vm->reg.rVal = val;

			vm->pc = vm->callStack[--vm->sp];

			for (int i = 0; i < 8; i++) {
				vm->reg.args[i] = vm->argStack[vm->sp][i];
				vm->reg.local[i] = vm->localStack[vm->sp][i];
			}
			break;
		}

		case OP_SYS: {
			u8 call = consume();
			DEREFPTR(arg1Ptr, call);

			if (call >= SYS_COUNT) {
				err("Invalid system call 0x%.2X", call);
				return;
			}
			strcat(debugLine, sysnames[call]);

			u8 args[8];
			for (int i = 0; i < 8; i++) {
				args[i] = vm->argStack[vm->sp][i];
				vm->argStack[vm->sp][i] = 0;
			}
			vm->argsp = 0;

			switch (call) {
				case SYS_DRAW:
					DrawTextureRec(
						vm->tileset,
						(Rectangle){args[0], args[1], args[2], args[3]},
						(Vector2){args[4], args[5]}, WHITE
					);
					break;

				case SYS_END:
					vm->needDraw = true;
					vm->reg.mouseX = GetMouseX() / vm->scale;
					vm->reg.mouseY = GetMouseY() / vm->scale;
					if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) vm->reg.mouseL++; else vm->reg.mouseL = 0;
					if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) vm->reg.mouseR++; else vm->reg.mouseR = 0;
					if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) vm->reg.up++; else vm->reg.up = 0;
					if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) vm->reg.down++; else vm->reg.down = 0;
					if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) vm->reg.left++; else vm->reg.left = 0;
					if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) vm->reg.right++; else vm->reg.right = 0;
					if (IsKeyDown(KEY_J)) vm->reg.act[0]++; else vm->reg.act[0] = 0;
					if (IsKeyDown(KEY_K)) vm->reg.act[1]++; else vm->reg.act[1] = 0;
					if (IsKeyDown(KEY_L)) vm->reg.act[2]++; else vm->reg.act[2] = 0;
					break;

				case SYS_SOUND: {
					if (args[0] > 3) {
						err("Invalid sound type %d", args[0]);
						return;
					}
					UnloadSound(vm->curSound[args[0]]);

					WaveParams params = {0};
					ResetWaveParams(&params);

					params.waveTypeValue = args[0];
					params.startFrequencyValue = (float) args[1] / 255;
					params.sustainTimeValue = (float) args[2] / 255;
					params.decayTimeValue = (float) args[3] / 255;

					Wave wave = GenerateWave(params);
					vm->curSound[args[0]] = LoadSoundFromWave(wave);
					PlaySound(vm->curSound[args[0]]);

					UnloadWave(wave);
					break;
				}
			}

			break;
		}
	}

	TraceLog(LOG_DEBUG, debugLine);

	if (vm->needDraw) {
		TraceLog(LOG_DEBUG, "_________________________________________________________________________");
		TraceLog(LOG_DEBUG, "");
	}
}