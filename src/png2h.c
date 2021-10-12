// Image to header file utility.
// Usage: ./png2h myimage.png - outputs myimage.h

#include "raylib.h"
#include <string.h>
#include <stdlib.h>

char *name;
Image img;

int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		img = LoadImage(argv[i]);
		name = TextReplace(argv[i], GetFileExtension(argv[i]), ".h");
		ExportImageAsCode(img, name);
		free(name);
		UnloadImage(img);
	}
}