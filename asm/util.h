#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

bool SaveFileData(const char *fileName, void *data, unsigned int bytesToWrite);
char *TextReplace(char *text, const char *replace, const char *by);

#endif // util.h