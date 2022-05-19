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
#include "../assets/intro.h"

#ifdef PLATFORM_WEB
	#include <emscripten/emscripten.h>
#else
	#include "tinyfiledialogs.h"
#endif

VM *vm;
bool showFps = false;
char message[33] = {0};
u8 msgTime = 0;
int speed = 60;
bool fileFromArgv = false;

Font font;

#define GXA_YELLOW (Color) {255, 208, 64, 255}

// _____________________________________________________________________________
//
//  Errors and Debugging
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
			char *dumpName = TextReplace(vm->fileName, GetFileExtension(vm->fileName), ".dmp");
			char *regDumpName = TextReplace(vm->fileName, GetFileExtension(vm->fileName), ".regs.dmp");
			SaveFileData(dumpName, vm->mem, 0x10000);
			SaveFileData(regDumpName, vm->reg.data, 0x100);
			free(dumpName);
			free(regDumpName);
		}
	#endif

	msgbox("Error", buf, "error");
	TraceLog(LOG_ERROR, "%s", buf);

	if (fileFromArgv) exit(EXIT_FAILURE);
	else {
		vm->state = ST_IDLE;
		vm->needDraw = true;
	}
}

// Ask for an address and value to write to memory.
// This has to be in a separate function so we can return to break out of both loops.
void debugWrite(void) {
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
void debugRead(void) {
	u16 addr = 0;
	while (true) {
		char *input = prompt("Debug read", "Input address to read (hex/dec/oct):");

		if (input == NULL) return;
		addr = strtoul(input, NULL, 0);
		if (errno != ERANGE) break;
	}

	char msgStr[64];
	sprintf(
		msgStr, "Value at 0x%.4X (%d):\n0x%.2X (%d)",
		addr, addr, vm->mem[addr], vm->mem[addr]
	);
	msgbox("Debug read", msgStr, "info");
}

#define SHOWMSG(...)\
	sprintf(message, __VA_ARGS__);\
	msgTime = 0;

// _____________________________________________________________________________
//
//  Loading/Unloading
// _____________________________________________________________________________
//
// Load a ROM file and tileset from memory.
void loadFileMem(u8 *file, unsigned int size, Image tileset) {
	if (!file) err("Failed to load file");
	if (size > 0x8000) err("ROM too large, 0x%.4X > 0x8000", size);

	if (file[0] != 'G' || file[1] != 'X' || file[2] != 'A') err("Invalid ROM file");

	// Copy ROM into gxarch memory, clear rest of gxarch memory, load SRAM, init registers
	memcpy(vm->rom, file, size);
	for (int i = size; i < 0x10000; i++) vm->mem[i] = 0;
	load(vm);

	for (int i = 0; i < 64; i++) vm->reg.data[i] = 0;
	vm->reg.rand = GetRandomValue(0, 0xFF);
	vm->pc = get16(rom, 3);

	vm->tileset = LoadTextureFromImage(tileset);
	UnloadImage(tileset);

	switch (speed) {
		case 60: SetWindowTitle("gxVM - running"); break;
		case 120: SetWindowTitle("gxVM - running 2x"); break;
		case 240: SetWindowTitle("gxVM - running 4x"); break;
	}
	vm->state = ST_RUNNING;
}

// Load a ROM file and tileset, if found.
void loadFile(char *name) {
	strcpy(vm->fileName, name);

	// Load ROM into memory
	unsigned int size;
	u8 *file = LoadFileData(name, &size);

	Image tileset;

	// Load tileset with the same filename as the ROM, for example:
	// ROM name is program.gxa, try to load program.png
	char *imgName = TextReplace(name, GetFileExtension(name), ".png");

	if (FileExists(imgName)) {
		tileset = LoadImage(imgName);

		if (tileset.width > 128 || tileset.height > 128) {
			UnloadImage(tileset);
			err(
				"Invalid tileset size, expected 128 × 128 but got %d × %d",
				tileset.width, tileset.height
			);
		}

		int palSize;
		Color *colors = LoadImagePalette(tileset, 17, &palSize);

		if (palSize > 16) {
			UnloadImagePalette(colors);
			UnloadImage(tileset);
			err("Tileset has too many colors, max 16");
		}

		for (int i = 0; i < palSize; i++) {
			if (colors[i].a != 0 && colors[i].a != 255) {
				UnloadImage(tileset);
				err(
					"Tileset color (%d, %d, %d, %d) has partial transparency, only alpha 0 or 255 is allowed",
					colors[i].r, colors[i].g, colors[i].b, colors[i].a
				);
			}
		}

		UnloadImagePalette(colors);
	} else {
		// If tileset image was not found, load the default tileset (tileset.h)
		TraceLog(LOG_WARNING, "%s not found, using default tileset", imgName);
		tileset = LoadImageFromMemory(".png", tileset_png, tileset_png_len);
	}

	loadFileMem(file, size, tileset);
	UnloadFileData(file);
	free(imgName);
}

// Unload everything and exit.
// Note: won't get run on Web, SRAM saving on Web is done with Alt + S
void cleanup() {
	#ifndef PLATFORM_WEB
		save(vm);

		for (int i = 0; i < 4; i++) UnloadSound(vm->curSound[i]);
		UnloadRenderTexture(vm->screen);
		UnloadTexture(vm->tileset);
		UnloadFont(font);
		free(vm);

		CloseWindow();
		CloseAudioDevice();
	#endif
}

// _____________________________________________________________________________
//
//  Read arguments
// _____________________________________________________________________________
//
void mainLoop(void);

int main(int argc, char **argv) {
	vm = malloc(sizeof(VM));
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
				vm->noSave = true;
			} else if (!strcmp(argv[i], "-dn") || !strcmp(argv[i], "-nd")) {
				vm->debug = true;
				vm->noSave = true;
			} else {
				// We can't use loadFile() here because window is not initialized
				// (we don't want to init window in case the arguments have --help)
				// Instead, the filename string is checked before the main loop.
				strcpy(vm->fileName, argv[i]);
				fileFromArgv = true;
			}
		}
	#endif

// _____________________________________________________________________________
//
//  Startup
// _____________________________________________________________________________
//
	SetTraceLogLevel(vm->debug ? LOG_DEBUG : LOG_WARNING);

	#ifdef PLATFORM_WEB
		// The web canvas fills the entire browser window, assume we're on at least 720p
		vm->scale = 6;
	#else
		vm->scale = 4;
	#endif

	InitWindow(SCREENW*vm->scale, SCREENH*vm->scale, "gxVM");
	InitAudioDevice();
	SetTargetFPS(speed);

	// ESC is a keycode in gxarch, it is also used to exit a raylib app by default
	// End can be used to exit gxarch, it also creates a memory dump in debug mode
	SetExitKey(0);

	// Window icon is only needed on Desktop, on Web the HTML shell's favicon is used
	#ifdef PLATFORM_DESKTOP
		Image icon = LoadImageFromMemory(".png", icon_png, icon_png_len);
		SetWindowIcon(icon);
	#endif

	vm->screen = LoadRenderTexture(SCREENW, SCREENH);

	// Load the gxarch font, only used for messages and the fps display
	Image fontImg = LoadImageFromMemory(".png", font_png, font_png_len);
	font = LoadFontFromImage(fontImg, MAGENTA, ' ');

	atexit(cleanup);

	// If a file was specified from command line args, load it
	if (fileFromArgv) loadFile(vm->fileName);

	#ifdef PLATFORM_WEB
		else loadFileMem(
			assets_intro_web_gxa, assets_intro_web_gxa_len,
			LoadImageFromMemory(".png", tileset_png, tileset_png_len)
		);
	#else
		else loadFileMem(
			assets_intro_gxa, assets_intro_gxa_len,
			LoadImageFromMemory(".png", tileset_png, tileset_png_len)
		);
	#endif

// _____________________________________________________________________________
//
//  Main loop
// _____________________________________________________________________________
//
	#ifdef PLATFORM_WEB
		emscripten_set_main_loop(mainLoop, 240, 1);
	#else
		while (!WindowShouldClose()) mainLoop();
	#endif

	exit(EXIT_SUCCESS);
}

void mainLoop(void) {
	if (IsFileDropped()) {
		int unused;
		char **files = GetDroppedFiles(&unused);
		loadFile(files[0]);
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
		if (vm->state == ST_IDLE || !strlen(vm->fileName)) {
			SHOWMSG("no program loaded");
		} else {
			loadFile(vm->fileName);
			SHOWMSG("reset");
		}
	}

	else if (IsKeyPressed(KEY_END)) {
		if (vm->debug) err("User initiated error");
		else exit(EXIT_SUCCESS);
	}

	else if (IsKeyPressed(KEY_PAUSE)) {
		switch (vm->state) {
			case ST_RUNNING:
				vm->state = ST_PAUSED;
				SHOWMSG("paused");
				SetWindowTitle("gxVM - paused");
				break;

			case ST_PAUSED:
				vm->state = ST_RUNNING;
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
			if (IsKeyPressed(KEY_F)) showFps = !showFps;
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
				if (file != NULL) loadFile(file);
			}

			else if (IsKeyPressed(KEY_F)) showFps = !showFps;
	#endif

		else if (IsKeyPressed(KEY_R)) debugRead();
		else if (IsKeyPressed(KEY_W)) debugWrite();

		else if (IsKeyPressed(KEY_B)) {
			vm->debug = !vm->debug;
			SetTraceLogLevel(vm->debug ? LOG_DEBUG : LOG_WARNING);
			SHOWMSG(vm->debug ? "debug on" : "debug off");
		}

		// cleanup() is used to save SRAM and unload everything before the window
		// closes, on Web we can't do that, so Alt + S is used to save instead
		#ifdef PLATFORM_WEB
			else if (IsKeyPressed(KEY_S)) {
				SHOWMSG("saved");
				save(vm);
			}
		#endif
	}

// _____________________________________________________________________________
//
//  Update and Draw
// _____________________________________________________________________________
//
	BeginTextureMode(vm->screen);
	if (vm->state != ST_PAUSED) {
		ClearBackground(BLACK);
		DrawTexturePro(
			vm->tileset,
			(Rectangle) {vm->reg.clearX, vm->reg.clearY, 1, 1},
			(Rectangle) {0, 0, 192, 160},
			(Vector2) {0, 0}, 0.0f, WHITE
		);
	}

	switch (vm->state) {
		case ST_RUNNING:
			while (!vm->needDraw) step(vm);
			// fall through

		case ST_PAUSED:
			vm->needDraw = false;
			break;

		case ST_IDLE: break;
	}

	// Show message for 1 second
	if (msgTime < speed) {
		// Draw a thick black outline with yellow text in the middle
		for (int x = 0; x < 3; x++) {
			for (int y = 0; y < 3; y++) {
				DrawTextEx(font, message, (Vector2) {x, y}, 8, 0, BLACK);
			}
		}
		DrawTextEx(font, message, (Vector2) {1, 1}, 8, 0, GXA_YELLOW);
		msgTime++;
	}

	if (showFps) {
		const char *fps = TextFormat("%d", GetFPS());
		for (int x = 0; x < 3; x++) {
			for (int y = 0; y < 3; y++) {
				DrawTextEx(font, fps, (Vector2) {x, 151 + y}, 8, 0, BLACK);
			}
		}
		DrawTextEx(font, fps, (Vector2) {1, 152}, 8, 0, GXA_YELLOW);
	}

	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);

	DrawTexturePro(
		vm->screen.texture,
		(Rectangle){0, 0, SCREENW, -SCREENH},
		(Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
		(Vector2){0, 0}, 0.0f, WHITE
	);

	EndDrawing();
}