#ifndef _promit_debug_
#define _promit_debug_

#pragma once

#include "chunk.h"

void disassembleChunk(Chunk*, const char*);
size_t disassembleInstruction(Chunk*, const size_t);

#endif
