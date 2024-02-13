#ifndef _promit_memory_
#define _promit_memory_

#pragma once

#include <stdlib.h>

#include "vm.h"

#define GROW_CAPACITY(x) (x < 64 ? 64 : x * 2)

#define FREE_ARRAY(type, pointer, oldSize) \
    reallocate((void*) pointer, sizeof(type) * (oldSize), 0u)

#define ALLOCATE(type, size) (type*) reallocate(NULL, 0u, (size) * sizeof(type))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0u)

#define GROW_ARRAY(type, pointer, oldSize, newSize) \
    (type*) reallocate((void*) pointer, sizeof(type) * (oldSize), sizeof(type) * (newSize))

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void setMemoryVM(VM*);
void markValue(Value);
void markObject(Obj*);
void garbageCollector();
void freeObject(Obj* obj);
void freeObjects();
void gcLibIgnore();
void gcVMIgnore(VM*);

#endif
