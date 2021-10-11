#include <stdlib.h>
#include <string.h>
#include "vm.h"
void err(const char *fmt, ...);

// Saves SRAM data to file.
void _save(struct VM *vm) {
	char *savename = TextReplace(vm->filename, GetFileExtension(vm->filename), ".sav");
	bool needsave = false;

	// If SRAM is blank and save file doesn't exist, no point in saving
	for (int i = SRAM; i < RESERVED; i++) if (vm->mem[i]) needsave = true;
	if (!needsave && !FileExists(savename)) return;

	SaveFileData(savename, vm->mem + SRAM, 0xF00);
	free(savename);
}

// Loads SRAM data from file.
void _load(struct VM *vm) {
	char *savename = TextReplace(vm->filename, GetFileExtension(vm->filename), ".sav");
	if (!FileExists(savename)) return;

	unsigned int size;
	u8 *data = LoadFileData(savename, &size);

	if (!data) err("Failed to load file");
	if (size > 0xF00) err("Save file too large, 0x%.4X > 0xF00", size);

	memcpy(vm->mem + SRAM, data, 0xF00);
	UnloadFileData(data);
}