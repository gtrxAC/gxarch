// Image to header file utility.
// Usage: ./png2h myimage.png - outputs myimage.h

#include "raylib.h"
#include <string.h>

const char *name;
Image img;

int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		img = LoadImage(argv[i]);

		name = GetFileNameWithoutExt(argv[i]);
		char outname[64] = {0};
		strcat(outname, name);
		strcat(outname, ".h");
		
		ExportImageAsCode(img, outname);
		UnloadImage(img);
	}
}