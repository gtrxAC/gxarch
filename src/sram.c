#include <stdlib.h>
#include <string.h>
#include "vm.h"
void err(const char *fmt, ...);

#ifdef PLATFORM_WEB
	#include <emscripten/emscripten.h>

	char *getfilename(void) {
		// gxVM is only given a file called rom.gxa, get the uploaded file's actual name
		return emscripten_run_script_string("document.querySelector('#rom').files[0].name");
	}
#endif

// Saves SRAM data to a file. localStorage is used on Web instead.
void _save(struct VM *vm) {
	#ifdef PLATFORM_WEB
		char script[(RESERVED - SRAM)*4 + 128] = "[";
		char num[4];

		for (int i = SRAM; i < RESERVED; i++) {
			sprintf(num, "%d", vm->mem[i]);
			strcat(script, num);
			strcat(script, ",");
		}

		strcat(script, TextFormat("].forEach((b, i) => localStorage.setItem(`%s_${i}`, b))", getfilename()));
		emscripten_run_script(script);
	#else
		if (vm->nosave) return;
		char *savename = TextReplace(vm->filename, GetFileExtension(vm->filename), ".sav");
		bool needsave = false;

		// If SRAM is blank and save file doesn't exist, no point in saving
		for (int i = SRAM; i < RESERVED; i++) if (vm->mem[i]) needsave = true;
		if (!needsave && !FileExists(savename)) return;

		SaveFileData(savename, vm->mem + SRAM, RESERVED - SRAM);
		free(savename);
	#endif
}

// Loads SRAM data from a file.
void _load(struct VM *vm) {
	#ifdef PLATFORM_WEB
		for (int i = 0; i < RESERVED - SRAM; i++) {
			vm->mem[SRAM + i] = emscripten_run_script_int(
				TextFormat("parseInt(localStorage.getItem('%s_%d'))", getfilename(), i));
		}
	#else
		if (vm->nosave) return;
		char *savename = TextReplace(vm->filename, GetFileExtension(vm->filename), ".sav");
		if (!FileExists(savename)) return;

		unsigned int size;
		u8 *data = LoadFileData(savename, &size);

		if (!data) err("Failed to load file");
		if (size > RESERVED - SRAM) err("Save file too large, 0x%.4X > 0xF00", size);

		memcpy(vm->mem + SRAM, data, RESERVED - SRAM);
		UnloadFileData(data);
	#endif
}