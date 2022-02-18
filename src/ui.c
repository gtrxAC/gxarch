#ifdef PLATFORM_DESKTOP
	#include <stddef.h>
	#include "tinyfiledialogs.h"
#endif

#ifdef PLATFORM_WEB
	#include <stdio.h>
	#include <emscripten/emscripten.h>
#endif

char *prompt(const char *title, const char *msg) {
	#ifdef PLATFORM_DESKTOP
		return tinyfd_inputBox(title, msg, " ");
	#elif defined(PLATFORM_WEB)
		char script[512];
		sprintf(script, "prompt(`%s`)", msg);
		return emscripten_run_script_string(script);
	#else
		return "";
	#endif
}

void msgbox(const char *title, const char *msg, const char *type) {
	#ifdef PLATFORM_DESKTOP
		tinyfd_messageBox(title, msg, "ok", type, 1);
	#elif defined(PLATFORM_WEB)
		char script[512];
		sprintf(script, "alert(`%s: %s`)", type, msg);
		emscripten_run_script(script);
	#endif
}

char *openfile(const char *title, const char *path, int filtercount, const char **filters, const char *filterdesc) {
	#ifdef PLATFORM_DESKTOP
		return tinyfd_openFileDialog(title, path, filtercount, filters, filterdesc, 0);
	#elif defined(PLATFORM_WEB)
		return NULL;
	#endif
}