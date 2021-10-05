#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "raylib.h"
#include "tinyfiledialogs.h"
#include "vm.h"

#include "../splash.h"
#include "../tileset.h"
#include "../icon.h"

enum {
	ST_IDLE,
	ST_RUNNING,
	ST_PAUSED
} state;

struct VM *vm;
RenderTexture screen;
Texture tileset;
char filename[256] = {0};
char message[64] = {0};
u8 msgtime = 0;
bool nosave = false;

// _____________________________________________________________________________
//
//  Errors & Debugging
// _____________________________________________________________________________
//

// Dump a range of memory into a file.
void dumprange(FILE *dump, u16 start, u16 end) {
	for (u16 i = start; i < end; i++) {
		fprintf(dump, "0x%.3x |", i);

		for (u8 j = 0; j < 16; j++) {
			fprintf(dump, "%.2x ", vm->mem[i * 16 + j]);
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
	
	tinyfd_notifyPopup("Error", buf, "error");
	exit(EXIT_FAILURE);
}

#define SHOWMSG(...)\
	sprintf(message, __VA_ARGS__);\
	msgtime = 0;

void draw() {
	for (u8 x = 0; x < 40; x++) {
		for (u8 y = 0; y < 25; y++) {
			u8 chr = vm->mem[VRAM + y * 40 + x];
			DrawTextureRec(
				tileset,
				(Rectangle){(chr % 16) * 8, ((u8) chr / 16) * 8, 8, 8},
				(Vector2){x * 8, y * 8}, WHITE
			);
		}
	}
}

// Saves SRAM data to file.
void save() {
	char *savename = TextReplace(filename, GetFileExtension(filename), ".sav");
	bool needsave = false;

	// If SRAM is blank and save file doesn't exist, no point in saving
	for (int i = SRAM; i < VRAM; i++) if (vm->mem[i]) needsave = true;
	if (!needsave && !FileExists(savename)) return;

	SaveFileData(savename, vm->mem + SRAM, 0x1000);
	free(savename);
}

// Loads SRAM data from file.
void load() {
	char *savename = TextReplace(filename, GetFileExtension(filename), ".sav");
	if (!FileExists(savename)) return;

	unsigned int size;
	u8 *data = LoadFileData(savename, &size);

	if (!data) err("Failed to load file");
	if (size > 0x1000) err("Save data too big, %d > 4096", size);

	memcpy(vm->mem + SRAM, data, 0x1000);
	UnloadFileData(data);
}

// _____________________________________________________________________________
//
//  Loading/Unloading
// _____________________________________________________________________________
//

// Unload everything and exit.
void cleanup() {
	save(vm);
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
	
	memcpy(&vm->mem, file, size);
	for (int i = size; i < 0x10000; i++) vm->mem[i] = 0;
	load(vm);

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
	vm = malloc(sizeof(struct VM));

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			puts("gxVM: gxarch emulator\n");
			puts("gxvm [options] [file]");
			puts("-h, --help    Show this message");
			puts("-d, --debug   Save memory dump on error");
			puts("-n, --nosave  Don't create a .sav file\n");
			puts("Keybinds:");
			puts("Ctrl + O      Open ROM");
			puts("End           Trigger an error, only works with --debug");
			puts("Page Up/Down  Resize screen");
			puts("Pause         Pause/continue emulation");
			exit(EXIT_SUCCESS);
		} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			vm->debug = true;
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
			SHOWMSG("%d x %d", width, height);
		}

		else if (IsKeyPressed(KEY_PAGE_DOWN)) {
			int width = GetScreenWidth() - 320;
			int height = GetScreenHeight() - 200;

			if (!width) width = 320, height = 200;

			SetWindowSize(width, height);
			SHOWMSG("%d x %d", width, height);
		}

		else if (IsKeyPressed(KEY_END) && vm->debug) err("User initiated error");

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
		
		else if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
			if (IsKeyPressed(KEY_O)) {
				char *file = tinyfd_openFileDialog("Open ROM", NULL, 0, NULL, NULL, 0);
				if (file != NULL) loadfile(file);
			}
		}

		// _____________________________________________________________________
		//
		//  Update and Draw
		// _____________________________________________________________________
		//

		if (state == ST_RUNNING) while (!vm->needdraw) step();

		BeginDrawing();
		ClearBackground(BLACK);
		BeginTextureMode(screen);

		if (vm->needdraw || state == ST_PAUSED) {
			draw();
			vm->needdraw = false;
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