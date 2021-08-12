#include "raylib.h"
#include "splash.h"
#include "tileset.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t
#define f32 float
#define f64 double

// _____________________________________________________________________________
//
//  Memory layout
// _____________________________________________________________________________
//

enum Instruction {
	I_NOP, I_SET, I_MOV,
	I_ADD, I_SUB, I_MUL, I_DIV,
	I_AND, I_OR, I_XOR, I_NOT,
	I_EQU, I_LT, I_GT,
	I_JMP, I_CJ, I_JS, I_CJS, I_RET,
	I_KEY, I_END
};

enum State {
	ST_IDLE,
	ST_RUNNING
};

struct VM {
	u8 mem[0x10000];
	RenderTexture screen;
	Texture tileset;
	enum State state;
	u8 needdraw;
} vm;

#define ENTRY 0x0000
#define STACK 0x8000
#define TILESET 0xB000
#define VRAM 0xF000
#define REG(n) (0xFFF0 + n)
#define RESULT 0xFFFA
#define RAND 0xFFFB
#define PC 0xFFFC
#define SP 0xFFFE

// _____________________________________________________________________________
//

char message[64] = {0};
u8 msgtime = 0;
u8 debug = 0;

#define showmsg(msg)\
	message = msg;\
	msgtime = 0;

#define fmtshowmsg(...)\
	sprintf(message, __VA_ARGS__);\
	msgtime = 0;

// _____________________________________________________________________________
//
//  Emulator
// _____________________________________________________________________________
//

// Dump a range of memory into a file.
void dumprange(FILE *dump, u16 start, u16 end) {
	for (u16 i = 0; i < 0x800; i++) {
		fprintf(dump, "0x%.3x |", i);

		for (u8 j = 0; j < 16; j++) {
			fprintf(dump, "%.2x ", vm.mem[i * 16 + j]);
		}

		fprintf(dump, "\n");
	}
}

// Print an error message with a memory dump and exit.
void err(const char *fmt, ...) {
	va_list args;

	printf("\nError: ");
	vprintf(fmt, args);
	printf("\n\n");

	if (!debug) printf("Tip: use --debug to get a memory dump\n\n");

	else {
		FILE *dump = fopen("dump.txt", "wt");

		if (dump) {
			fprintf(dump, "ROM/RAM\n");
			dumprange(dump, 0x000, 0x800);
			fprintf(dump, "Call stack");
			dumprange(dump, 0x800, 0x810);
			fprintf(dump, "Tileset");
			dumprange(dump, 0xB00, 0xF00);
			fprintf(dump, "VRAM");
			dumprange(dump, 0xF00, 0xF3F);
			fprintf(dump, "Registers");
			dumprange(dump, 0xFFF, 0x1000);
			fclose(dump);
		}
	}
	
	exit(EXIT_FAILURE);
}

// _____________________________________________________________________________
//

// Set a 16-bit value.
void set16(u16 index, u16 val) {
	u8 high = (val & 0xFF00) >> 8;
	u8 low = val & 0xFF;
	vm.mem[index] = high;
	vm.mem[index + 1] = low;
}

// Get a 16-bit value.
u16 get16(u16 index) {
	return vm.mem[index] << 8 | vm.mem[index + 1];
}

// Get the next instruction byte.
u8 consume(void) {
	u8 res = vm.mem[get16(PC)];
	set16(PC, get16(PC) + 1);
	return res;
}

// Get the next instruction double (16 bits).
u16 consume16(void) {
	return consume() << 8 | consume();
}

// Get a value argument.
u8 getval(u8 ptr) {
	return ptr ? vm.mem[consume16()] : consume();
}

// Get an address argument.
u16 getaddr(u8 ptr) {
	u16 result = consume16();
	if (ptr) result = get16(result);
	return result;
}

// _____________________________________________________________________________
//

// Run one instruction.
void step(void) {
	u8 inst = consume();
	u8 arg1ptr = inst & 0b10000000;
	u8 arg2ptr = inst & 0b01000000;

	switch (inst & 0b00111111) {
		#define dbgins(i) //printf("0x%.4x/%d %s\n", get16(PC) - 1, get16(PC) - 1, i)

		case I_NOP:
			dbgins("nop");
			break;

		case I_SET: {
			dbgins("set");
			vm.mem[getaddr(arg1ptr)] = getval(arg2ptr);
			break;
		}

		case I_MOV: {
			dbgins("mov");
			u16 src = getaddr(arg1ptr);
			u16 dest = getaddr(arg2ptr);
			vm.mem[dest] = vm.mem[src];
			break;
		}

		#define BINOP(op) {\
			dbgins(#op);\
			vm.mem[RESULT] = getval(arg1ptr) op getval(arg2ptr);\
			break;\
		}

		case I_ADD: BINOP(+)
		case I_SUB: BINOP(-)
		case I_MUL: BINOP(*)
		case I_DIV: BINOP(/)
		case I_AND: BINOP(&)
		case I_OR: BINOP(|)
		case I_XOR: BINOP(^)

		case I_NOT:
			vm.mem[RESULT] = ~getval(arg1ptr);
			break;

		case I_EQU: BINOP(==)
		case I_LT: BINOP(<)
		case I_GT: BINOP(>)

		case I_JMP:
			dbgins("jmp ");
			set16(PC, getaddr(arg1ptr));
			break;

		case I_CJ: {
			dbgins("cj");
			u8 cond = vm.mem[getaddr(arg1ptr)];
			u16 addr = getaddr(arg2ptr);
			if (cond) set16(PC, addr);
			break;
		}

		case I_JS:
			dbgins("js");
			set16(get16(SP), get16(PC) + 1);
			set16(SP, get16(SP) + 2);
			set16(PC, getaddr(arg1ptr));
			break;

		case I_CJS: {
			dbgins("cjs");
			u8 cond = vm.mem[getaddr(arg1ptr)];
			u16 addr = getaddr(arg2ptr);
			if (cond) {
				set16(get16(SP), get16(PC) + 1);
				set16(SP, get16(SP) + 2);
				set16(PC, addr);
			}
			break;
		}

		case I_RET:
			dbgins("ret");
			set16(PC, get16(get16(SP)));
			set16(SP, get16(SP) - 2);

			if (get16(SP) < 0x8000)
				err("Call stack underflow, 0x%.4x < 0x8000", get16(SP));

			break;

		case I_END:
			vm.needdraw = true;
			break;

		case I_KEY:
			vm.mem[RESULT] = IsKeyPressed(getval(arg1ptr));
			break;

		default:
			err("Invalid opcode or not implemented: %d", inst);
			break;
	}
}

void draw(void) {
	for (i8 x = 0; x < 40; x++) {
		for (i8 y = 0; y < 25; y++) {
			u8 chr = vm.mem[VRAM + y * 40 + x];
			DrawTextureRec(
				vm.tileset,
				(Rectangle){(chr % 16) * 8, ((u8) chr / 16) * 8, 8, 8},
				(Vector2){x * 8, y * 8}, WHITE
			);
		}
	}
}

// _____________________________________________________________________________
//
//  Loading/Unloading
// _____________________________________________________________________________
//

// Unload everything and exit.
void cleanup(void) {
	UnloadRenderTexture(vm.screen);
	UnloadTexture(vm.tileset);
	CloseWindow();
}

// Load a ROM file and tileset, if found.
void loadfile(char *name) {
	int size;
	u8 *file = LoadFileData(name, &size);

	if (!file) err("Failed to load file");

	if (size > 0x8000) err("ROM too big, %d > 32768", size);
	
	memcpy(&vm.mem, file, size);
	for (u32 i = size; i < 0x10000; i++) vm.mem[i] = 0;
	set16(PC, get16(0x0000));
	set16(STACK, get16(0x0000));
	set16(SP, STACK + 2);

	UnloadFileData(file);

	// Load tileset with the same filename as the ROM, for example:
	// ROM name is program.bin, try to load program.png
	char *imgname = TextReplace(name, GetFileExtension(name), ".png");

	if (FileExists(imgname)) {
		vm.tileset = LoadTexture(imgname);

		if (vm.tileset.width != 128 || vm.tileset.height != 128)
			err(
				"Invalid tileset size, expected 128 x 128 but got %d x %d",
				vm.tileset.width, vm.tileset.height
			);
	} else {
		// If tileset image was not found, load the default tileset (tileset.h)
		TraceLog(LOG_WARNING, "%s not found, using default tileset", imgname);

		Image tilesetimg = {
			TILESET_DATA,
			TILESET_WIDTH,
			TILESET_HEIGHT,
			1, TILESET_FORMAT
		};

		vm.tileset = LoadTextureFromImage(tilesetimg);
	}

	ClearDroppedFiles();
	free(imgname);
	SetWindowTitle("gxVM - running");
	vm.state = ST_RUNNING;
}

// _____________________________________________________________________________
//
//  Startup
// _____________________________________________________________________________
//

int main(int argc, char **argv) {
	InitWindow(640, 400, "gxVM");
	SetTargetFPS(60);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			debug = true;
		} else {
			loadfile(argv[i]);
		}
	}

	vm.screen = LoadRenderTexture(320, 200);

	// Load splash screen from header file
	// Default tileset is loaded if a ROM doesn't have a tileset, see loadfile()
	Image splashimg = {
		SPLASH_DATA,
		SPLASH_WIDTH,
		SPLASH_HEIGHT,
		1, SPLASH_FORMAT
	};

	Texture splash = LoadTextureFromImage(splashimg);

	atexit(cleanup);
	
	while (!WindowShouldClose()) {
		if (vm.state == ST_IDLE && IsFileDropped()) {
			int unused;
			char **files = GetDroppedFiles(&unused);
			loadfile(files[0]);
		}

		// _____________________________________________________________________
		//
		//  Screen Resizing
		// _____________________________________________________________________
		//

		if (IsKeyPressed(KEY_PAGE_UP)) {
			int width = GetScreenWidth() + 320;
			int height = GetScreenHeight() + 200;

			SetWindowSize(width, height);
			fmtshowmsg("%d x %d", width, height);
		}

		if (IsKeyPressed(KEY_PAGE_DOWN)) {
			int width = GetScreenWidth() - 320;
			int height = GetScreenHeight() - 200;

			if (!width) width = 320, height = 200;

			SetWindowSize(width, height);
			fmtshowmsg("%d x %d", width, height);
		}

		if (IsKeyPressed(KEY_END) && debug) err("User initiated error");

		// _____________________________________________________________________
		//
		//  Update and Draw
		// _____________________________________________________________________
		//

		if (vm.state == ST_RUNNING) while (!vm.needdraw) step();

		BeginDrawing();
		ClearBackground(BLACK);

		if (vm.state == ST_RUNNING) {
			if (vm.needdraw) {
				BeginTextureMode(vm.screen);
				draw();
				EndTextureMode();
				vm.needdraw = false;
			}
			DrawTexturePro(
				vm.screen.texture,
				(Rectangle){0, 0, vm.screen.texture.width, -vm.screen.texture.height},
				(Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
				(Vector2){0, 0}, 0.0f, WHITE
			);
		} else {
			// If no file loaded, show "drag and drop ROM file" screen
			DrawTextureEx(splash, (Vector2){0, 0}, 0.0f, GetScreenWidth() / 320, WHITE);
		}

		// Show message for 1 second
		if (msgtime < 60) {
			DrawText(message, 1, 1, 10 * GetScreenWidth() / 320, BLACK);
			DrawText(message, 0, 0, 10 * GetScreenWidth() / 320, WHITE);
			msgtime++;
		}

		EndDrawing();
	}

	exit(EXIT_SUCCESS);
}