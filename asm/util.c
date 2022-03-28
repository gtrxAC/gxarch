#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Save data to file from buffer
bool SaveFileData(const char *fileName, void *data, unsigned int bytesToWrite)
{
	if (!fileName) return false;

	FILE *file = fopen(fileName, "wb");
	if (!file) return false;

	unsigned int count = (unsigned int)fwrite(data, sizeof(unsigned char), bytesToWrite, file);

	if (count == 0 || count != bytesToWrite) return false;
	return !fclose(file);
}

// Replace text string
// REQUIRES: strlen(), strstr(), strncpy(), strcpy()
// WARNING: Returned buffer must be freed by the user (if return != NULL)
char *TextReplace(char *text, const char *replace, const char *by)
{
    // Sanity checks and initialization
    if (!text || !replace || !by) return NULL;

    char *result = NULL;

    char *insertPoint = NULL;   // Next insert point
    char *temp = NULL;          // Temp pointer
    int replaceLen = 0;         // Replace string length of (the string to remove)
    int byLen = 0;              // Replacement length (the string to replace replace by)
    int lastReplacePos = 0;     // Distance between replace and end of last replace
    int count = 0;              // Number of replacements

    replaceLen = strlen(replace);
    if (replaceLen == 0) return NULL;  // Empty replace causes infinite loop during count

    byLen = strlen(by);

    // Count the number of replacements needed
    insertPoint = text;
    for (count = 0; (temp = strstr(insertPoint, replace)); count++) insertPoint = temp + replaceLen;

    // Allocate returning string and point temp to it
    temp = result = (char *)malloc(strlen(text) + (byLen - replaceLen)*count + 1);

    if (!result) return NULL;   // Memory could not be allocated

    // First time through the loop, all the variable are set correctly from here on,
    //  - 'temp' points to the end of the result string
    //  - 'insertPoint' points to the next occurrence of replace in text
    //  - 'text' points to the remainder of text after "end of replace"
    while (count--)
    {
        insertPoint = strstr(text, replace);
        lastReplacePos = (int)(insertPoint - text);
        temp = strncpy(temp, text, lastReplacePos) + lastReplacePos;
        temp = strcpy(temp, by) + byLen;
        text += lastReplacePos + replaceLen; // Move to next "end of replace"
    }

    // Copy remaind text part after replacement to result (pointed by moving temp)
    strcpy(temp, text);

    return result;
}