#ifndef _promit_compiler_
#define _promit_compiler_

#pragma once

#include "vm.h"

ObjFunction* compile(VM*, const char*);
void markCompilerRoots();

#endif
