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
void save(VM *vm) {
	#ifdef PLATFORM_WEB
		char script[16384 + 128] = "[";
		char num[4];

		for (int i = 0; i < 0x1000; i++) {
			sprintf(num, "%d", vm->sram[i]);
			strcat(script, num);
			strcat(script, ",");
		}

		strcat(script, TextFormat("].forEach((b, i) => localStorage.setItem(`%s_${i}`, b))", getfilename()));
		emscripten_run_script(script);
	#else
		if (vm->noSave) return;
		char *saveName = TextReplace(vm->fileName, GetFileExtension(vm->fileName), ".sav");

		// If SRAM is blank and save file doesn't exist, no point in saving
		bool needSave = false;
		for (int i = 0; i < 0x1000; i++) {
			if (vm->sram[i]) {
				needSave = true;
				break;
			}
		}

		if (needSave || FileExists(saveName)) SaveFileData(saveName, vm->sram, 0x1000);
		free(saveName);
	#endif
}

// Loads SRAM data from a file.
void load(VM *vm) {
	if (vm->state == ST_IDLE) return;
	
	#ifdef PLATFORM_WEB
		for (int i = 0; i < 0x1000; i++) {
			vm->sram[i] = emscripten_run_script_int(
				TextFormat("parseInt(localStorage.getItem('%s_%d'))", getfilename(), i));
		}
	#else
		if (vm->noSave) return;
		char *saveName = TextReplace(vm->fileName, GetFileExtension(vm->fileName), ".sav");
		if (!FileExists(saveName)) return;

		unsigned int size;
		u8 *data = LoadFileData(saveName, &size);

		if (!data) err("Failed to load file");
		if (size > 0x1000) err("Save file too large, 0x%.4X > 0x1000", size);

		memcpy(vm->sram, data, 0x1000);
		UnloadFileData(data);
	#endif
}