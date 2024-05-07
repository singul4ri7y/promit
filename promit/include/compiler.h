#ifndef _promit_compiler_
#define _promit_compiler_

#pragma once

#include "vm.h"
#include "chunk.h"

ObjFunction* compile(VM*, const char*, const char*);
void markCompilerRoots();

#endif
