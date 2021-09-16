#include "raylib.h"
#include "splash.h"
#include "tileset.h"
#include "icon.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define u8 uint8_t
#define u16 uint16_t

// _____________________________________________________________________________
//
//  Memory layout
// _____________________________________________________________________________
//

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
	'`', 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, // 0x7F
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

enum Instruction {
	I_NOP, I_SET, I_MOV,
	I_ADD, I_SUB, I_MUL, I_DIV,
	I_AND, I_OR, I_XOR, I_NOT,
	I_EQU, I_LT, I_GT,
	I_JMP, I_CJ, I_JS, I_CJS, I_RET,
	I_KEY, I_END
};

enum {
	ST_IDLE,
	ST_RUNNING,
	ST_PAUSED
} state;

u8 mem[0x10000];
RenderTexture screen;
Texture tileset;
bool needdraw;

#define ENTRY 0x0000
#define SRAM 0xE000
#define VRAM 0xF000
#define STACK 0xF400
#define REG(n) (0xFFE0 + n)
#define RESULT 0xFFF0
#define RAND 0xFFFB
#define PC 0xFFFC
#define SP 0xFFFE

// _____________________________________________________________________________
//

char filename[256] = {0};
char message[64] = {0};
u8 msgtime = 0;
bool nosave = false;
bool debug = false;
bool capslock = false;

#define showmsg(...)\
	sprintf(message, __VA_ARGS__);\
	msgtime = 0;

// _____________________________________________________________________________
//
//  Emulator
// _____________________________________________________________________________
//

// Dump a range of memory into a file.
void dumprange(FILE *dump, u16 start, u16 end) {
	for (u16 i = start; i < end; i++) {
		fprintf(dump, "0x%.3x |", i);

		for (u8 j = 0; j < 16; j++) {
			fprintf(dump, "%.2x ", mem[i * 16 + j]);
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
			dumprange(dump, 0x000, 0xDFF);
			fprintf(dump, "\n\nSave RAM\n");
			dumprange(dump, 0xE00, 0xEFF);
			fprintf(dump, "\n\nVideo RAM\n");
			dumprange(dump, 0xF00, 0xF3E);
			fprintf(dump, "\n\nCall stack\n");
			dumprange(dump, 0xF40, 0xF4F);
			fprintf(dump, "\n\nRegisters\n");
			dumprange(dump, 0xFFF, 0x1000);
			fclose(dump);
		} else {
			perror("Failed to write dump file");
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
	mem[index] = high;
	mem[index + 1] = low;
}

// Get a 16-bit value.
u16 get16(u16 index) {
	return mem[index] << 8 | mem[index + 1];
}

// Get the next instruction byte.
u8 consume(void) {
	u8 res = mem[get16(PC)];
	set16(PC, get16(PC) + 1);
	return res;
}

// Get the next instruction double (16 bits).
u16 consume16(void) {
	return consume() << 8 | consume();
}

// Get a value argument.
u8 getval(u8 ptr) {
	return ptr ? mem[consume16()] : consume();
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
	// Update random number register.
	mem[RAND] = GetRandomValue(0, 0xFF);

	if (get16(PC) < 2 || get16(PC) > REG(0))
		err("Attempted to execute code at 0x%.4x", get16(PC));

	u8 inst = consume();
	u8 arg1ptr = inst & 0b10000000;
	u8 arg2ptr = inst & 0b01000000;

	switch (inst & 0b00111111) {
		#define DBGINS(i) if (debug) printf("0x%.4x/%d %s\n", get16(PC) - 1, get16(PC) - 1, i)
		#define INST(i) case I_ ## i: DBGINS(#i);

		INST(NOP) break;

		INST(SET) {
			mem[getaddr(arg1ptr)] = getval(arg2ptr);
			break;
		}

		INST(MOV) {
			u16 src = getaddr(arg1ptr);
			u16 dest = getaddr(arg2ptr);
			mem[dest] = mem[src];
			break;
		}

		#define BINOP(name, op) \
			INST(name) {\
				set16(RESULT, getval(arg1ptr) op getval(arg2ptr));\
				break;\
			}

		BINOP(ADD, +)
		BINOP(SUB, -)
		BINOP(MUL, *)
		BINOP(DIV, /)
		BINOP(AND, &)
		BINOP(OR, |)
		BINOP(XOR, ^)

		INST(NOT)
			mem[RESULT + 1] = ~getval(arg1ptr);
			break;

		BINOP(EQU, ==)
		BINOP(LT, <)
		BINOP(GT, >)

		INST(JMP) {
			set16(PC, getaddr(arg1ptr));
			break;
		}

		INST(CJ) {
			u8 cond = mem[getaddr(arg1ptr)];
			u16 addr = getaddr(arg2ptr);
			if (cond) set16(PC, addr);
			break;
		}

		INST(JS) {
			set16(get16(SP), get16(PC) + 1);
			set16(SP, get16(SP) + 2);
			set16(PC, getaddr(arg1ptr));
			break;
		}

		INST(CJS) {
			u8 cond = mem[getaddr(arg1ptr)];
			u16 addr = getaddr(arg2ptr);
			
			if (cond) {
				set16(get16(SP), get16(PC));
				set16(SP, get16(SP) + 2);
				set16(PC, addr);
			}
			break;
		}

		INST(RET) {
			set16(get16(SP), 0);
			set16(SP, get16(SP) - 2);
			set16(PC, get16(get16(SP))); // -1 or not -1?

			if (get16(SP) < STACK)
				err("Call stack underflow, 0x%.4x < 0x%.4x", get16(SP), STACK);

			break;
		}

		INST(END) needdraw = true; break;

		INST(KEY) {
			mem[RESULT + 1] = IsKeyDown(keymap[getval(arg1ptr)]);
			break;
		}

		default:
			err("Invalid opcode at 0x%.4x: %d", get16(PC), inst);
			break;
	}
}

void draw(void) {
	for (u8 x = 0; x < 40; x++) {
		for (u8 y = 0; y < 25; y++) {
			u8 chr = mem[VRAM + y * 40 + x];
			DrawTextureRec(
				tileset,
				(Rectangle){(chr % 16) * 8, ((u8) chr / 16) * 8, 8, 8},
				(Vector2){x * 8, y * 8}, WHITE
			);
		}
	}
}

// Saves SRAM data to file.
void save(void) {
	char *savename = TextReplace(filename, GetFileExtension(filename), ".sav");
	bool needsave = false;

	// If SRAM is blank and save file doesn't exist, no point in saving
	for (int i = SRAM; i < VRAM; i++) if (mem[i]) needsave = true;
	if (!needsave && !FileExists(savename)) return;

	SaveFileData(savename, mem + SRAM, 0x1000);
	free(savename);
}

// Loads SRAM data from file.
void load(void) {
	char *savename = TextReplace(filename, GetFileExtension(filename), ".sav");
	if (!FileExists(savename)) return;

	unsigned int size;
	u8 *data = LoadFileData(savename, &size);

	if (!data) err("Failed to load file");
	if (size > 0x1000) err("Save data too big, %d > 4096", size);

	memcpy(mem + SRAM, data, 0x1000);
	UnloadFileData(data);
}

// _____________________________________________________________________________
//
//  Loading/Unloading
// _____________________________________________________________________________
//

// Unload everything and exit.
void cleanup(void) {
	save();
	UnloadRenderTexture(screen);
	UnloadTexture(tileset);
	CloseWindow();
}

// Load a ROM file and tileset, if found.
void loadfile(char *name) {
	strcpy(filename, name);

	unsigned int size;
	u8 *file = LoadFileData(name, &size);

	if (!file) err("Failed to load file");
	if (size > 0xE000) err("ROM too big, %d > 56k", size);
	
	memcpy(&mem, file, size);
	for (int i = size; i < 0x10000; i++) mem[i] = 0;
	load();

	set16(PC, get16(0x0000));
	set16(STACK, get16(0x0000));
	set16(SP, STACK + 2);

	UnloadFileData(file);

	// Load tileset with the same filename as the ROM, for example:
	// ROM name is program.bin, try to load program.png
	char *imgname = TextReplace(name, GetFileExtension(name), ".png");

	if (FileExists(imgname)) {
		tileset = LoadTexture(imgname);

		if (tileset.width != 128 || tileset.height != 128)
			err(
				"Invalid tileset size, expected 128 x 128 but got %d x %d",
				tileset.width, tileset.height
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

		tileset = LoadTextureFromImage(tilesetimg);
	}

	free(imgname);
	SetWindowTitle("gxVM - running");
	state = ST_RUNNING;
}

// _____________________________________________________________________________
//
//  Startup
// _____________________________________________________________________________
//

int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			puts("gxVM: gxarch emulator\n");
			puts("gxvm [options] [file]");
			puts("-h, --help    Show this message");
			puts("-d, --debug   Save memory dump on error");
			puts("-n, --nosave  Don't create a .sav file\n");
			puts("Keybinds:");
			puts("End           Trigger an error, only works with --debug");
			puts("Page Up/Down  Resize screen");
			puts("Pause         Pause/continue emulation");
			exit(EXIT_SUCCESS);
		} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			debug = true;
		} else if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--nosave")) {
			nosave = true;
		} else {
			// We can't use loadfile() here because window is not initialized
			// Instead, the filename string is checked before the main loop.
			strcpy(filename, argv[i]);
		}
	}

	InitWindow(640, 400, "gxVM");
	SetTargetFPS(60);

	Image icon = {
		ICON_DATA,
		ICON_WIDTH,
		ICON_HEIGHT,
		1, ICON_FORMAT
	};

	SetWindowIcon(icon);

	screen = LoadRenderTexture(320, 200);

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

	// If a file was specified from command line args, load it
	if (state == ST_IDLE && strlen(filename)) loadfile(filename);
	
	while (!WindowShouldClose()) {
		if (state == ST_IDLE && IsFileDropped()) {
			int unused;
			char **files = GetDroppedFiles(&unused);
			loadfile(files[0]);
			ClearDroppedFiles();
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
			showmsg("%d x %d", width, height);
		}

		else if (IsKeyPressed(KEY_PAGE_DOWN)) {
			int width = GetScreenWidth() - 320;
			int height = GetScreenHeight() - 200;

			if (!width) width = 320, height = 200;

			SetWindowSize(width, height);
			showmsg("%d x %d", width, height);
		}

		else if (IsKeyPressed(KEY_END) && debug) err("User initiated error");

		else if (IsKeyPressed(KEY_PAUSE)) {
			switch (state) {
				case ST_RUNNING:
					state = ST_PAUSED;
					showmsg("paused");
					SetWindowTitle("gxVM - paused");
					break;

				case ST_PAUSED:
					state = ST_RUNNING;
					showmsg("running");
					SetWindowTitle("gxVM - running");
					break;

				case ST_IDLE:
					showmsg("no program loaded");
					break;
			}
		}

		// Note: if caps lock is turned on before gxvm is started, this will be
		// out of sync - I can't do much about that
		else if (IsKeyPressed(KEY_CAPS_LOCK)) {
			capslock = !capslock;
			showmsg("caps lock %s", capslock ? "on" : "off");
		}

		// _____________________________________________________________________
		//
		//  Update and Draw
		// _____________________________________________________________________
		//

		if (state == ST_RUNNING) while (!needdraw) step();

		BeginDrawing();
		ClearBackground(BLACK);
		BeginTextureMode(screen);

		if (needdraw || state == ST_PAUSED) {
			draw();
			needdraw = false;
		} else if (state == ST_IDLE) {
			DrawTexture(splash, 0, 0, WHITE);
		}

		// Show message for 1 second
		if (msgtime < 60) {
			// Draw a thick black outline with yellow text in the middle
			for (int x = 0; x < 3; x++) {
				for (int y = 0; y < 3; y++) {
					DrawText(message, x, y, 10, BLACK);
				}
			}
			DrawText(message, 1, 1, 10, YELLOW);
			msgtime++;
		}

		EndTextureMode();

		DrawTexturePro(
			screen.texture,
			(Rectangle){0, 0, screen.texture.width, -screen.texture.height},
			(Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
			(Vector2){0, 0}, 0.0f, WHITE
		);

		EndDrawing();
	}

	exit(EXIT_SUCCESS);
}