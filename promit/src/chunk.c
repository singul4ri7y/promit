#include <string.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
	chunk -> capacity    = 0u;
	chunk -> count       = 0u;
	chunk -> code        = NULL;
	chunk -> lineSize    = 0u;
	chunk -> highestLine = 0u;
	chunk -> lines       = NULL;

	initValueArray(&chunk -> constants);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
	if(chunk -> count + 1u > chunk -> capacity) {
		size_t oldCapacity = chunk -> capacity;

		chunk -> capacity = GROW_CAPACITY(oldCapacity);
		chunk -> code     = GROW_ARRAY(uint8_t, chunk -> code, oldCapacity, chunk -> capacity);
	}

	if(line > chunk -> highestLine) {
		chunk -> highestLine = line;
		chunk -> lineSize++;

		chunk -> lines = (Line*) realloc(chunk -> lines, chunk -> lineSize * sizeof(Line));
		
		chunk -> lines[chunk -> lineSize - 1].offset = chunk -> lineSize >= 2 ? chunk -> lines[chunk -> lineSize - 2].offset : 0;
		chunk -> lines[chunk -> lineSize - 1].line   = line;
	}

	chunk -> code[chunk -> count++] = byte;
	chunk -> lines[chunk -> lineSize - 1u].offset++;
}

void writeConstant(Chunk* chunk, Value value, int line) {
	size_t size = chunk -> constants.count;

	if(size >= (0xFFFFFF + 1u)) {
		fprintf(stderr, "[Error][Fatal][Line %d]: Too many constants!", line);
		exit(-1);
	}

	if(size >= 256u) {
		writeChunk(chunk, OP_CONSTANT_LONG, line);
		size = writeValueArray(&chunk -> constants, value);

		int b4 = (int) size;

		uint8_t c = b4 & 0xFF;

		b4 >>= 0x8;

		uint8_t b = b4 & 0xFF;

		b4 >>= 0x8;

		uint8_t a = b4 & 0xFF;

		writeChunk(chunk, a, line);
		writeChunk(chunk, b, line);
		writeChunk(chunk, c, line);

		return;
	}

	writeChunk(chunk, OP_CONSTANT, line);
	writeChunk(chunk, (uint8_t) writeValueArray(&chunk -> constants, value), line);
}

void freeChunk(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk -> code, chunk -> capacity);

	freeValueArray(&chunk -> constants);
	free(chunk -> lines);

	initChunk(chunk);
}

// At least a genuine line number will come out of it, that's ensured.

int getLine(Chunk* chunk, size_t offset) {
	offset++;

	int left = 0, right = chunk -> lineSize - 1;
	int ptr;

	find: 
		ptr = left + (right - left) / 2;

		Line current = chunk -> lines[ptr];

		int ptrl = ptr - 1, ptrr = ptr + 1;

		if(offset <= current.offset && ptrl >= left) {
			if(offset > chunk -> lines[ptrl].offset) goto ret;
			else if(offset == chunk -> lines[ptrl].offset) return chunk -> lines[ptrl].line;
			else {
				right = ptrl;
				
				goto find;
			}
		}

		if(offset > current.offset && ptrr <= right) {
			if(offset <= chunk -> lines[ptrr].offset) return chunk -> lines[ptrr].line;
			else {
				left = ptrr;

				goto find;
			}
		}

		ret: 

		if(offset <= current.offset) 
			return current.line;

	return -1;
}
