#ifndef _promit_lib_
#define _promit_lib_

#pragma once

#include "object.h"
#include "compiler.h"
#include "utilities.h"

#define RUNTIME_ERROR(format, ...) runtimeError(vm, format, ##__VA_ARGS__)

void initSystemLib(VM*);
void initFileLib(VM*);
void initMathLib(VM*);
void initTimeLib(VM*);
void initNumberLib(VM*);
void initListLib(VM*);
void initStringLib(VM*);
void initDictionaryLib(VM*);
void initFunctionLib(VM*);

char* toString(VM*, Value* const);

#endif