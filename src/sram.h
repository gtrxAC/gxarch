#ifndef SRAM_H
#define SRAM_H

#include "vm.h"

void _save(struct VM *vm);
void _load(struct VM *vm);

#define save() _save(vm)
#define load() _load(vm)

#endif // sram.h