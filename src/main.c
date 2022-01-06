#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "raylib.h"
#include "tinyfiledialogs.h"
#include "vm.h"
#include "sram.h"

#include "../assets/splash.h"
#include "../assets/tileset.h"
#include "../assets/icon.h"

enum {
	ST_IDLE,
	ST_RUNNING,
	ST_PAUSED
} state;

struct VM *vm;
bool showfps = false;
char message[64] = {0};
u8 msgtime = 0;
int speed = 60;

// _____________________________________________________________________________
//
//  Errors & Debugging
// _____________________________________________________________________________
//

// Dump a range of memory into a file.
// Note: start/end should be divided by 16
// For example, to dump 0xF000 - 0xFEFF, use 0xF00 for start and 0xFEF for end.
void dumprange(FILE *dump, u16 start, u16 end) {
	for (u16 i = start; i < end; i++) {
		fprintf(dump, "0x%.3x |", i);

		for (u8 j = 0; j < 16; j++) {
			fprintf(dump, "%.2X ", vm->mem[i * 16 + j]);
		}

		fprintf(dump, "\n");
	}
}

// Print an error message with a memory dump and exit.
void err(const char *fmt, ...) {
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	if (!vm->debug) {
		strcat(buf, "\nTip: use --debug to get a memory dump");
	} else {
		FILE *dump = fopen("dump.txt", "wt");

		if (dump) {
			fprintf(dump, "ROM/RAM\n");
			dumprange(dump, 0x000, 0xEFF);
			fprintf(dump, "\n\nSave RAM\n");
			dumprange(dump, 0xF00, 0xFEF);
			fprintf(dump, "\n\nReserved\n");
			dumprange(dump, 0xFF0, 0x1000);
			fclose(dump);
		} else {
			perror("Failed to write dump file");
		}
	}
	
	tinyfd_notifyPopup("Error", buf, "error");
	fprintf(stderr, "%s\n", buf);
	exit(EXIT_FAILURE);
}

#define SHOWMSG(...)\
	sprintf(message, __VA_ARGS__);\
	msgtime = 0;

// _____________________________________________________________________________
//
//  Loading/Unloading
// _____________________________________________________________________________
//

// Unload everything and exit.
void cleanup() {
	if (vm->mem[SRAM_TOGGLE]) save();
	UnloadRenderTexture(vm->screen);
	UnloadTexture(vm->tileset);
	CloseWindow();
	free(vm);
}

// Load a ROM file and tileset, if found.
void loadfile(char *name) {
	strcpy(vm->filename, name);

	unsigned int size;
	u8 *file = LoadFileData(name, &size);

	if (!file) err("Failed to load file");
	if (size > 0xFF00) err("ROM too large, 0x%.4X > 0xFF00", size);
	
	memcpy(&vm->mem, file, size);
	for (int i = size; i < 0x10000; i++) vm->mem[i] = 0;
	load();
	vm->mem[RAND] = GetRandomValue(0, 0xFF);

	vm->pc = get16(0x0000);

	UnloadFileData(file);

	// Load tileset with the same filename as the ROM, for example:
	// ROM name is program.bin, try to load program.png
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
		UnloadImagePalette(colors);

		if (palsize > 16) {
			UnloadImage(tileset);
			err("Tileset has too many colors, max 16");
		}

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
	SetWindowTitle("gxVM - running");
	state = ST_RUNNING;
}

// _____________________________________________________________________________
//
//  Startup
// _____________________________________________________________________________
//

int main(int argc, char **argv) {
	vm = malloc(sizeof(struct VM));
	if (vm == NULL) err("Failed to allocate virtual machine");
	
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
			puts("Ctrl + R      Reset");
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
			// Instead, the filename string is checked before the main loop.
			strcpy(vm->filename, argv[i]);
		}
	}

	vm->scale = 4;
	SetTraceLogLevel(vm->debug ? LOG_INFO : LOG_WARNING);
	InitWindow(512, 512, "gxVM");
	SetTargetFPS(speed);

	// ESC is a keycode in gxarch, it is also used to exit a raylib app by default
	// End can be used to exit gxarch, it also creates a memory dump in debug mode
	SetExitKey(0);

	Image icon = {
		ICON_DATA,
		ICON_WIDTH,
		ICON_HEIGHT,
		1, ICON_FORMAT
	};

	SetWindowIcon(icon);

	vm->screen = LoadRenderTexture(SCREENW, SCREENH);

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
	if (state == ST_IDLE && strlen(vm->filename)) loadfile(vm->filename);

	// _________________________________________________________________________
	//
	//  Main loop
	// _________________________________________________________________________
	//
	
	while (!WindowShouldClose()) {
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
		
		else if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
			if (IsKeyPressed(KEY_O)) {
				char const *filter[1] = {"*.gxa"};
				char path[512];
				strcpy(path, GetWorkingDirectory());
				strcat(path, "/*");

				char *file = tinyfd_openFileDialog("Open ROM", path, 1, filter, "gxarch ROMs", 0);
				if (file != NULL) loadfile(file);
			}

			else if (IsKeyPressed(KEY_F)) showfps = !showfps;

			else if (IsKeyPressed(KEY_R)) {
				loadfile(vm->filename);
				SHOWMSG("reset");
			}
		}

		// _____________________________________________________________________
		//
		//  Update and Draw
		// _____________________________________________________________________
		//

		BeginTextureMode(vm->screen);
		if (state != ST_PAUSED) ClearBackground(BLACK);

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

		if (showfps) {
			const char *fps = TextFormat("%d", GetFPS());
			for (int x = 0; x < 3; x++) {
				for (int y = 0; y < 3; y++) {
					DrawText(fps, x, 117 + y, 10, BLACK);
				}
			}
			DrawText(fps, 1, 118, 10, YELLOW);
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

	exit(EXIT_SUCCESS);
}