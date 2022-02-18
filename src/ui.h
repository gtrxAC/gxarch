#ifndef UI_H
#define UI_H

char *prompt(const char *title, const char *msg);
void msgbox(const char *title, const char *msg, const char *type);
char *openfile(const char *title, const char *path, int filtercount, const char **filters, const char *filterdesc);

#endif // ui.h