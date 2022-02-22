#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "raylib.h"
#include "ui.h"
#include "vm.h"
#include "sram.h"

#include "../assets/tileset.h"
#include "../assets/icon.h"
#include "../assets/font.h"

#ifdef PLATFORM_WEB
	#include <emscripten/emscripten.h>
	#include "../assets/splash_web.h"
#else
	#include "tinyfiledialogs.h"
	#include "../assets/splash.h"
#endif

enum {
	ST_IDLE,
	ST_RUNNING,
	ST_PAUSED
} state;

struct VM *vm;
bool showfps = false;
char message[33] = {0};
u8 msgtime = 0;
int speed = 60;
bool filefromargv = false;

Texture splash;
Font font;
Color clear;

#define GXA_YELLOW (Color) {255, 208, 64, 255}

// _____________________________________________________________________________
//
//  Errors & Debugging
// _____________________________________________________________________________
//

// Print an error message with a memory dump and exit.
void err(const char *fmt, ...) {
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	#ifndef PLATFORM_WEB
		if (!vm->debug) {
			strcat(buf, "\nTip: use --debug to get a memory dump");
		} else {
			SaveFileData("dump.bin", vm->mem, 0x10000);
		}
	#endif

	msgbox("Error", buf, "error");
	fprintf(stderr, "%s\n", buf);

	if (filefromargv) exit(EXIT_FAILURE);
	else {
		state = ST_IDLE;
		vm->needdraw = true;
	}
}

// Ask for an address and value to write to memory.
// This has to be in a separate function so we can return to break out of both loops.
void debugwrite(void) {
	u16 addr = 0;
	u8 val = 0;
	while (true) {
		char *input = prompt("Debug write", "Input address to write to (hex/dec/oct):");

		if (input == NULL) return;
		addr = strtoul(input, NULL, 0);
		if (errno != ERANGE) break;
	}
	while (true) {
		char *input = prompt("Debug write", "Input value to write (hex/dec/oct):");

		if (input == NULL) return;
		val = strtoul(input, NULL, 0);
		if (errno != ERANGE) break;
	}
	vm->mem[addr] = val;
}

// Ask for an address and show its value to the user.
void debugread(void) {
	u16 addr = 0;
	while (true) {
		char *input = prompt("Debug read", "Input address to read (hex/dec/oct):");

		if (input == NULL) return;
		addr = strtoul(input, NULL, 0);
		if (errno != ERANGE) break;
	}

	char msgstr[64];
	sprintf(
		msgstr, "Value at 0x%.4X (%d):\n0x%.2X (%d)",
		addr, addr, vm->mem[addr], vm->mem[addr]
	);
	msgbox("Debug read", msgstr, "info");
}

#define SHOWMSG(...)\
	sprintf(message, __VA_ARGS__);\
	msgtime = 0;

// _____________________________________________________________________________
//
//  Loading/Unloading
// _____________________________________________________________________________
//

// Load a ROM file and tileset, if found.
void loadfile(char *name) {
	strcpy(vm->filename, name);

	// Load ROM into RAM
	unsigned int size;
	u8 *file = LoadFileData(name, &size);

	if (!file) err("Failed to load file");
	if (size > 0xFF00) err("ROM too large, 0x%.4X > 0xFF00", size);

	// Copy ROM into gxarch RAM, clear rest of gxarch RAM, load SRAM, init registers
	memcpy(&vm->mem, file, size);
	for (int i = size; i < 0x10000; i++) vm->mem[i] = 0;
	if (file[SRAM_TOGGLE]) load();
	vm->mem[RAND] = GetRandomValue(0, 0xFF);

	for (int i = 0; i < 32; i++) vm->reg[i] = 0;
	vm->pc = get16(0x0000);

	UnloadFileData(file);

	// Load tileset with the same filename as the ROM, for example:
	// ROM name is program.gxa, try to load program.png
	char *imgname = TextReplace(name, GetFileExtension(name), ".png");

	if (FileExists(imgname)) {
		int palsize;
		Image tileset = LoadImage(imgname);

		if (tileset.width > 128 || tileset.height > 128) {
			UnloadImage(tileset);
			err(
				"Invalid tileset size, expected 128 × 128 but got %d × %d",
				tileset.width, tileset.height
			);
		}

		Color *colors = LoadImagePalette(tileset, 17, &palsize);

		if (palsize > 16) {
			UnloadImagePalette(colors);
			UnloadImage(tileset);
			err("Tileset has too many colors, max 16");
		}

		for (int i = 0; i < palsize; i++) {
			if (colors[i].a != 0 && colors[i].a != 255) {
				UnloadImage(tileset);
				err(
					"Tileset color (%d, %d, %d, %d) has partial transparency, only alpha 0 or 255 is allowed",
					colors[i].r, colors[i].g, colors[i].b, colors[i].a
				);
			}
		}

		UnloadImagePalette(colors);
		vm->tileset = LoadTextureFromImage(tileset);
		UnloadImage(tileset);
	} else {
		// If tileset image was not found, load the default tileset (tileset.h)
		TraceLog(LOG_WARNING, "%s not found, using default tileset", imgname);

		Image tilesetimg = {
			TILESET_DATA,
			TILESET_WIDTH,
			TILESET_HEIGHT,
			1, TILESET_FORMAT
		};

		vm->tileset = LoadTextureFromImage(tilesetimg);
	}

	free(imgname);
	switch (speed) {
		case 60: SetWindowTitle("gxVM - running"); break;
		case 120: SetWindowTitle("gxVM - running 2x"); break;
		case 240: SetWindowTitle("gxVM - running 4x"); break;
	}
	clear = (Color) {vm->mem[CLEAR_R], vm->mem[CLEAR_G], vm->mem[CLEAR_B], 255};
	state = ST_RUNNING;
}

// Unload everything and exit.
// Note: won't get run on Web, SRAM saving on Web is done with Alt + S
void cleanup() {
	#ifndef PLATFORM_WEB
		if (vm->mem[SRAM_TOGGLE]) save();

		for (int i = 0; i < 4; i++) UnloadSound(vm->cursound[i]);
		UnloadRenderTexture(vm->screen);
		UnloadTexture(vm->tileset);
		free(vm);

		CloseWindow();
		CloseAudioDevice();
	#endif
}

// _____________________________________________________________________________
//
//  Startup
// _____________________________________________________________________________
//

void mainloop(void);

int main(int argc, char **argv) {
	vm = malloc(sizeof(struct VM));
	if (vm == NULL) err("Failed to allocate virtual machine");

	#ifndef PLATFORM_WEB
		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				puts("gxVM: gxarch emulator\n");
				puts("Usage: gxvm [options] [file]");
				puts("-h, --help    Show this message");
				puts("-d, --debug   Save memory dump on error");
				puts("-n, --nosave  Don't create a .sav file\n");
				puts("Keybinds:");
				puts("Ctrl + O      Open ROM");
				puts("Ctrl + F      Show/hide FPS");
				puts("Ctrl + R      Debug read memory from address");
				puts("Ctrl + W      Debug write value to address");
				puts("Home          Reset");
				puts("End           Exit, creates a memory dump in debug mode");
				puts("Page Up/Down  Resize screen");
				puts("Pause         Pause/continue emulation");
				puts("Insert        Fast forward");
				exit(EXIT_SUCCESS);
			} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
				vm->debug = true;
			} else if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--nosave")) {
				vm->nosave = true;
			} else if (!strcmp(argv[i], "-dn") || !strcmp(argv[i], "-nd")) {
				vm->debug = true;
				vm->nosave = true;
			} else {
				// We can't use loadfile() here because window is not initialized
				// (we don't want to init window in case the arguments have --help)
				// Instead, the filename string is checked before the main loop.
				strcpy(vm->filename, argv[i]);
				filefromargv = true;
			}
		}
	#endif

	SetTraceLogLevel(vm->debug ? LOG_INFO : LOG_WARNING);

	#ifdef PLATFORM_WEB
		// The web canvas fills the entire browser window, assume we're on at least 720p
		// Title is not needed for web, the title from the HTML shell is used
		vm->scale = 6;
		InitWindow(768, 768, "");
	#else
		vm->scale = 4;
		InitWindow(512, 512, "gxVM");
	#endif
	InitAudioDevice();
	SetTargetFPS(speed);

	// ESC is a keycode in gxarch, it is also used to exit a raylib app by default
	// End can be used to exit gxarch, it also creates a memory dump in debug mode
	SetExitKey(0);

	// Window icon is only needed on Desktop, on Web the HTML shell's favicon is used
	#ifdef PLATFORM_DESKTOP
		Image icon = {
			ICON_DATA,
			ICON_WIDTH,
			ICON_HEIGHT,
			1, ICON_FORMAT
		};

		SetWindowIcon(icon);
	#endif

	vm->screen = LoadRenderTexture(SCREENW, SCREENH);

	// Load splash screen from header file
	// Default tileset is loaded if a ROM doesn't have a tileset, see loadfile()
	Image splashimg = {
		#ifdef PLATFORM_WEB
			SPLASH_WEB_DATA,
			SPLASH_WEB_WIDTH,
			SPLASH_WEB_HEIGHT,
			1, SPLASH_WEB_FORMAT
		#else
			SPLASH_DATA,
			SPLASH_WIDTH,
			SPLASH_HEIGHT,
			1, SPLASH_FORMAT
		#endif
	};
	splash = LoadTextureFromImage(splashimg);

	// Load the gxarch font, only used for messages and the fps display
	Image fontimg = {
		FONT_DATA,
		FONT_WIDTH,
		FONT_HEIGHT,
		1, FONT_FORMAT
	};
	font = LoadFontFromImage(fontimg, MAGENTA, ' ');

	atexit(cleanup);

	// If a file was specified from command line args, load it
	if (filefromargv) loadfile(vm->filename);

	// _________________________________________________________________________
	//
	//  Main loop
	// _________________________________________________________________________
	//

	#ifdef PLATFORM_WEB
		emscripten_set_main_loop(mainloop, 240, 1);
	#else
		while (!WindowShouldClose()) mainloop();
	#endif

	exit(EXIT_SUCCESS);
}

void mainloop(void) {
	if (state == ST_IDLE && IsFileDropped()) {
		int unused;
		char **files = GetDroppedFiles(&unused);
		loadfile(files[0]);
		ClearDroppedFiles();
	}

	if (IsKeyPressed(KEY_PAGE_UP)) {
		vm->scale++;
		SetWindowSize(SCREENW * vm->scale, SCREENH * vm->scale);
		SHOWMSG("%d x %d", SCREENW * vm->scale, SCREENH * vm->scale);
	}

	else if (IsKeyPressed(KEY_PAGE_DOWN)) {
		vm->scale--;
		if (!vm->scale) vm->scale = 1;
		SetWindowSize(SCREENW * vm->scale, SCREENH * vm->scale);
		SHOWMSG("%d x %d", SCREENW * vm->scale, SCREENH * vm->scale);
	}

	else if (IsKeyPressed(KEY_HOME)) {
		if (state == ST_IDLE) {
			SHOWMSG("no program loaded");
		} else {
			loadfile(vm->filename);
			SHOWMSG("reset");
		}
	}

	else if (IsKeyPressed(KEY_END)) {
		if (vm->debug) err("User initiated error");
		else exit(EXIT_SUCCESS);
	}

	else if (IsKeyPressed(KEY_PAUSE)) {
		switch (state) {
			case ST_RUNNING:
				state = ST_PAUSED;
				SHOWMSG("paused");
				SetWindowTitle("gxVM - paused");
				break;

			case ST_PAUSED:
				state = ST_RUNNING;
				SHOWMSG("running");
				SetWindowTitle("gxVM - running");
				break;

			case ST_IDLE:
				SHOWMSG("no program loaded");
				break;
		}
	}

	else if (IsKeyPressed(KEY_INSERT)) {
		switch (speed) {
			case 60:
				speed = 120;
				SHOWMSG("2x speed");
				SetWindowTitle("gxVM - running 2x");
				break;

			case 120:
				speed = 240;
				SHOWMSG("4x speed");
				SetWindowTitle("gxVM - running 4x");
				break;

			case 240:
				speed = 60;
				SHOWMSG("normal speed");
				SetWindowTitle("gxVM - running");
				break;
		}
		SetTargetFPS(speed);
	}

	#ifdef PLATFORM_WEB
		// Use Alt on Web, Ctrl may interfere with browser's keyboard shortcuts
		else if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
			if (IsKeyPressed(KEY_F)) showfps = !showfps;
	#else
		else if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
			// Open file functionality is not needed on Web, the HTML shell takes
			// care of that
			if (IsKeyPressed(KEY_O)) {
				char const *filter[1] = {"*.gxa"};
				char path[512];
				strcpy(path, GetWorkingDirectory());
				strcat(path, "/*");

				char *file = tinyfd_openFileDialog("Open ROM", path, 1, filter, "gxarch ROMs", 0);
				if (file != NULL) loadfile(file);
			}

			else if (IsKeyPressed(KEY_F)) showfps = !showfps;
	#endif

		else if (IsKeyPressed(KEY_R)) debugread();
		else if (IsKeyPressed(KEY_W)) debugwrite();

		// cleanup() is used to save SRAM and unload everything before the window
		// closes, on Web we can't do that, so Alt + S is used to save instead
		#ifdef PLATFORM_WEB
			else if (IsKeyPressed(KEY_S)) {
				if (vm->mem[SRAM_TOGGLE]) {
					SHOWMSG("saved");
					save();
				} else {
					SHOWMSG("program doesn't support saves");
				}
			}
		#endif
	}

	// _________________________________________________________________________
	//
	//  Update and Draw
	// _________________________________________________________________________
	//

	BeginTextureMode(vm->screen);
	if (state != ST_PAUSED) ClearBackground(clear);

	switch (state) {
		case ST_IDLE:
			DrawTexture(splash, 0, 0, WHITE);
			break;

		case ST_RUNNING:
			while (!vm->needdraw) step();
			// fall through

		case ST_PAUSED:
			vm->needdraw = false;
			break;
	}

	// Show message for 1 second
	if (msgtime < speed) {
		// Draw a thick black outline with yellow text in the middle
		for (int x = 0; x < 3; x++) {
			for (int y = 0; y < 3; y++) {
				DrawTextEx(font, message, (Vector2) {x, y}, 8, 0, BLACK);
			}
		}
		DrawTextEx(font, message, (Vector2) {1, 1}, 8, 0, GXA_YELLOW);
		msgtime++;
	}

	if (showfps) {
		const char *fps = TextFormat("%d", GetFPS());
		for (int x = 0; x < 3; x++) {
			for (int y = 0; y < 3; y++) {
				DrawTextEx(font, fps, (Vector2) {x, 119 + y}, 8, 0, BLACK);
			}
		}
		DrawTextEx(font, fps, (Vector2) {1, 120}, 8, 0, GXA_YELLOW);
	}

	EndTextureMode();

	BeginDrawing();
	ClearBackground(clear);

	DrawTexturePro(
		vm->screen.texture,
		(Rectangle){0, 0, SCREENW, -SCREENH},
		(Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
		(Vector2){0, 0}, 0.0f, WHITE
	);

	EndDrawing();
}