#include "debug.h"
#include "object.h"

void disassembleChunk(Chunk* chunk, const char* name, const char* type) {
	printf("\n== '%s' (%s)==\n\n", name, type);
	printf(" OFFSET   -    LINE  -  OPCODE     -     CONSTANT  -  VALUE\n\n");


	for(size_t offset = 0u; offset < chunk -> count; ) 
		offset = disassembleInstruction(chunk, offset);
}

static uint32_t getParameter(Chunk* chunk, size_t offset, bool isLong) {
	uint32_t constant = chunk -> code[offset + 1];

	if(isLong) {
		constant <<= 0x8;

		constant += chunk -> code[offset + 2u];

		constant <<= 0x8;

		constant += chunk -> code[offset + 3u];
	}

	return constant;
}

static size_t simpleInstruction(const char* opcode, size_t offset) {
	printf("     %s\n", opcode);

	return offset + 1u;
}

static size_t constantInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint8_t constant = getParameter(chunk, offset, false);

	printf("     %-16s 0x%06x     ", opcode, constant);
	printValue(&chunk -> constants.values[constant]);
	printf("\n");

	return offset + 2u;
}

static size_t globalLongInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint8_t param     = getParameter(chunk, offset, false);
	uint32_t constant = getParameter(chunk, offset + 1, true);

	printf("     %-16s 0x%06x     ", opcode, constant);
	printValue(&chunk -> constants.values[constant]);
	printf("      %d <-", param);
	printf("\n");

	return offset + 5u;
}

static size_t globalInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint8_t param    = getParameter(chunk, offset, false);
	uint8_t constant = getParameter(chunk, offset + 1, false);

	printf("     %-16s 0x%06x     ", opcode, constant);
	printValue(&chunk -> constants.values[constant]);
	printf("      %d <-", param);
	printf("\n");

	return offset + 3u;
}

static size_t constantLongInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint32_t constant = getParameter(chunk, offset, true);

	printf("     %-16s 0x%06x     ", opcode, constant);
	printValue(&chunk -> constants.values[constant]);
	printf("\n");

	return offset + 4u;
}

static size_t byteInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint32_t slot = getParameter(chunk, offset, false);

	printf("     %-16s 0x%06x     \n", opcode, slot);

	return offset + 2u;
}

static size_t byteLongInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint32_t slot = getParameter(chunk, offset, true);

	printf("     %-16s 0x%06x     \n", opcode, slot);

	return offset + 4u;
}

static size_t jumpInstruction(const char* opcode, Chunk* chunk, size_t offset, char sign) {
	uint32_t jump = getParameter(chunk, offset, true);

	printf("     %-16s 0x%06lx -> 0x%06lx\n", opcode, offset, offset + 4 + sign * jump);

	return offset + 4u;
}

static size_t increamentInstruction(const char* opcode, Chunk* chunk, size_t offset) {
	uint8_t type = getParameter(chunk, offset, false);

    printf("     %-16s 0x%06x     ", opcode, type);

    return offset + 1u;
}

static size_t closureInstruction(const char* opcode, Chunk* chunk, size_t offset, bool isLong) {
	int constant = getParameter(chunk, offset, isLong);

	printf("     %-16s 0x%06x     ", opcode, constant);
	printValue(&chunk -> constants.values[constant]);
	printf("\n");	

	ObjFunction* function = VALUE_FUNCTION(chunk -> constants.values[constant]);

	offset += isLong ? 3u : 1u;

	for(int i = 0; i < function -> upvalueCount; i++) {
		bool isLocal = getParameter(chunk, offset, false);
		int index    = getParameter(chunk, offset + 1u, true);

		offset += 4u;

		printf("0x%06lx          |                                         %s %d\n", 
			offset - 2, isLocal ? "local" : "upvalue", index);
	}

	return offset + 1u;
}

static size_t localInstruction(const char* opcode, uint8_t instruction, uint8_t stride, size_t offset) {
	uint8_t slot = instruction - stride;

	printf("     %s%-3d 0x%06x     \n", opcode, slot, slot);

	return offset + 1u;
}

static size_t invokeInstruction(const char* opcode, Chunk* chunk, size_t offset) {
    uint8_t constant = getParameter(chunk, offset, false);
    bool isStatic = getParameter(chunk, offset + 1u, false);
    uint8_t argc  = getParameter(chunk, offset + 2u, false);

    printf("     %-16s 0x%06x     ", opcode, constant);
    printValue(chunk -> constants.values + constant);
    printf("     (%d) [%s]\n", argc, isStatic ? "STATIC" : "NOT-STATIC");

    return offset + 4u;
}

size_t disassembleInstruction(Chunk* chunk, const size_t offset) {
	printf("0x%06lx     ", offset);

	int line = getLine(chunk, offset);

	printf("%6d", line);

	uint8_t ins = chunk -> code[offset];

	switch(ins) {
		case OP_CONSTANT: 
			return constantInstruction("OP_CONSTANT", chunk, offset);

		case OP_CONSTANT_LONG: 
			return constantLongInstruction("OP_CONSTANT_LONG", chunk, offset);

		case OP_TRUE: 
			return simpleInstruction("OP_TRUE", offset);

		case OP_FALSE: 
			return simpleInstruction("OP_FALSE", offset);

		case OP_NULL: 
			return simpleInstruction("OP_NULL", offset);

		case OP_NEGATE: 
			return simpleInstruction("OP_NEGATE", offset);

		case OP_NOT: 
			return simpleInstruction("OP_NOT", offset);

		case OP_BITWISE_NEGATION: 
			return simpleInstruction("OP_BITWISE_NEGATION" , offset);

		case OP_GREATER: 
			return simpleInstruction("OP_GREATER", offset);

		case OP_LESS: 
			return simpleInstruction("OP_LESS", offset);

		case OP_GREATER_EQUAL: 
			return simpleInstruction("OP_GREATER_EQUAL", offset);

		case OP_LESS_EQUAL: 
			return simpleInstruction("OP_LESS_EQUAL", offset);

		case OP_INFINITY: 
			return simpleInstruction("OP_INFINITY", offset);

		case OP_NAN: 
			return simpleInstruction("OP_NAN", offset);

		case OP_SHOW: 
			return simpleInstruction("OP_SHOW", offset);

		case OP_SHOWL: 
			return simpleInstruction("OP_SHOWL", offset);

		case OP_ADD: 
			return simpleInstruction("OP_ADD", offset);

		case OP_SUBSTRACT: 
			return simpleInstruction("OP_SUBSTRACT", offset);

		case OP_MULTIPLY: 
			return simpleInstruction("OP_MULTIPLY", offset);

		case OP_DIVIDE: 
			return simpleInstruction("OP_DIVIDE", offset);

		case OP_MODULUS: 
			return simpleInstruction("OP_MODULUS", offset);

		case OP_EQUAL: 
			return simpleInstruction("OP_EQUAL", offset);

		case OP_NOT_EQUAL: 
			return simpleInstruction("OP_NOT_EQUAL", offset);

		case OP_DEFINE_GLOBAL: 
			return globalInstruction("OP_DEFINE_GLOBAL", chunk, offset);

		case OP_DEFINE_GLOBAL_LONG: 
			return globalLongInstruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);

		case OP_GET_GLOBAL: 
			return constantInstruction("OP_GET_GLOBAL", chunk, offset);

		case OP_GET_GLOBAL_LONG: 
			return constantLongInstruction("OP_GET_GLOBAL_LONG", chunk, offset);

		case OP_SET_GLOBAL: 
			return constantInstruction("OP_SET_GLOBAL", chunk, offset);

		case OP_SET_GLOBAL_LONG: 
			return constantLongInstruction("OP_SET_GLOBAL_LONG", chunk, offset);

		case OP_GET_LOCAL: 
			return byteInstruction("OP_GET_LOCAL", chunk, offset);

		case OP_GET_LOCAL_LONG: 
			return byteLongInstruction("OP_GET_LOCAL_LONG", chunk, offset);

		case OP_SET_LOCAL: 
			return byteInstruction("OP_SET_LOCAL", chunk, offset);

		case OP_SET_LOCAL_LONG: 
			return byteLongInstruction("OP_SET_LOCAL_LONG", chunk, offset);

		case OP_POP: 
			return simpleInstruction("OP_POP", offset);

		case OP_SILENT_POP: 
			return simpleInstruction("OP_SILENT_POP", offset);

		case OP_JUMP_IF_FALSE: 
			return jumpInstruction("OP_JUMP_IF_FALSE", chunk, offset, 1);

		case OP_JUMP_OPR: 
			return jumpInstruction("OP_JUMP_OPR", chunk, offset, 1);

		case OP_JUMP: 
			return jumpInstruction("OP_JUMP", chunk, offset, 1);

		case OP_LOOP: 
			return jumpInstruction("OP_LOOP", chunk, offset, -1);

		case OP_TYPEOF: 
			return simpleInstruction("OP_TYPEOF", offset);

		case OP_DUP: 
			return simpleInstruction("OP_DUP", offset);

		case OP_POST_INCREMENT: 
			return increamentInstruction("OP_POST_INCREMENT", chunk, offset);

		case OP_POST_DECREMENT: 
			return increamentInstruction("OP_POST_DECREMENT", chunk, offset);

		case OP_PRE_INCREMENT: 
			return increamentInstruction("OP_PRE_INCREMENT", chunk, offset);

		case OP_PRE_DECREMENT: 
			return increamentInstruction("OP_PRE_DECREMENT", chunk, offset);

		case OP_RECEIVE: 
			return increamentInstruction("OP_RECIEVE", chunk, offset + 1u);

		case OP_CALL: 
			return byteInstruction("OP_CALL", chunk, offset);

		case OP_CLOSURE: 
			return closureInstruction("OP_CLOSURE", chunk, offset, false);

		case OP_CLOSURE_LONG: 
			return closureInstruction("OP_CLOSRUE_LONG", chunk, offset, true);

		case OP_GET_UPVALUE: 
			return byteInstruction("OP_GET_UPVALUE", chunk, offset);

		case OP_GET_UPVALUE_LONG: 
			return byteLongInstruction("OP_GET_UPVALUE_LONG", chunk, offset);

		case OP_SET_UPVALUE: 
			return byteInstruction("OP_SET_UPVALUE", chunk, offset);

		case OP_SET_UPVALUE_LONG: 
			return byteLongInstruction("OP_SET_UPVALUE_LONG", chunk, offset);

		case OP_CLOSE_UPVALUE: 
			return simpleInstruction("OP_CLOSE_UPVALUE", offset);

		case OP_CLASS: 
			return constantInstruction("OP_CLASS", chunk, offset);

		case OP_CLASS_LONG: 
			return constantLongInstruction("OP_CLASS_LONG", chunk, offset);

		case OP_SET_PROPERTY: 
			return constantInstruction("OP_SET_PROPERTY", chunk, offset);

		case OP_SET_PROPERTY_LONG: 
			return constantLongInstruction("OP_SET_PROPERTY_LONG", chunk, offset);

		case OP_GET_PROPERTY: 
			return constantInstruction("OP_GET_PROPERTY", chunk, offset);

		case OP_GET_PROPERTY_LONG: 
			return constantLongInstruction("OP_GET_PROPERTY_LONG", chunk, offset);

		case OP_DNM_GET_PROPERTY: 
			return simpleInstruction("OP_DNM_GET_PROPERTY", offset);

		case OP_DNM_SET_PROPERTY: 
			return simpleInstruction("OP_DNM_SET_PROPERTY", offset);

		case OP_METHOD: 
			return constantInstruction("OP_METHOD", chunk, offset);

		case OP_METHOD_LONG: 
			return constantInstruction("OP_METHOD_LONG", chunk, offset);

		case OP_ADD_LIST: 
			return simpleInstruction("OP_ADD_LIST", offset);

		case OP_GET_LOCAL_0:
		case OP_GET_LOCAL_1:
		case OP_GET_LOCAL_2:
		case OP_GET_LOCAL_3:
		case OP_GET_LOCAL_4:
		case OP_GET_LOCAL_5:
		case OP_GET_LOCAL_6:
		case OP_GET_LOCAL_7:
		case OP_GET_LOCAL_8:
		case OP_GET_LOCAL_9:
		case OP_GET_LOCAL_10:
		case OP_GET_LOCAL_11:
		case OP_GET_LOCAL_12:
		case OP_GET_LOCAL_13:
		case OP_GET_LOCAL_14:
		case OP_GET_LOCAL_15:
		case OP_GET_LOCAL_16:
		case OP_GET_LOCAL_17:
		case OP_GET_LOCAL_18:
		case OP_GET_LOCAL_19:
		case OP_GET_LOCAL_20: 
			return localInstruction("OP_GET_LOCAL_", ins, OP_GET_LOCAL_0, offset);

		case OP_SET_LOCAL_0:
		case OP_SET_LOCAL_1:
		case OP_SET_LOCAL_2:
		case OP_SET_LOCAL_3:
		case OP_SET_LOCAL_4:
		case OP_SET_LOCAL_5:
		case OP_SET_LOCAL_6:
		case OP_SET_LOCAL_7:
		case OP_SET_LOCAL_8:
		case OP_SET_LOCAL_9:
		case OP_SET_LOCAL_10:
		case OP_SET_LOCAL_11:
		case OP_SET_LOCAL_12:
		case OP_SET_LOCAL_13:
		case OP_SET_LOCAL_14:
		case OP_SET_LOCAL_15:
		case OP_SET_LOCAL_16:
		case OP_SET_LOCAL_17:
		case OP_SET_LOCAL_18:
		case OP_SET_LOCAL_19:
		case OP_SET_LOCAL_20: 
			return localInstruction("OP_SET_LOCAL_", ins, OP_SET_LOCAL_0, offset);

		case OP_CALL_0:
		case OP_CALL_1:
		case OP_CALL_2:
		case OP_CALL_3:
		case OP_CALL_4:
		case OP_CALL_5:
		case OP_CALL_6:
		case OP_CALL_7:
		case OP_CALL_8:
		case OP_CALL_9:
		case OP_CALL_10:
		case OP_CALL_11:
		case OP_CALL_12:
		case OP_CALL_13:
		case OP_CALL_14:
		case OP_CALL_15:
		case OP_CALL_16:
		case OP_CALL_17:
		case OP_CALL_18:
		case OP_CALL_19:
		case OP_CALL_20: 
			return localInstruction("OP_CALL_", ins, OP_CALL_0, offset);

		case OP_GET_UPVALUE_0:
		case OP_GET_UPVALUE_1:
		case OP_GET_UPVALUE_2:
		case OP_GET_UPVALUE_3:
		case OP_GET_UPVALUE_4:
		case OP_GET_UPVALUE_5:
		case OP_GET_UPVALUE_6:
		case OP_GET_UPVALUE_7:
		case OP_GET_UPVALUE_8:
		case OP_GET_UPVALUE_9:
		case OP_GET_UPVALUE_10:
		case OP_GET_UPVALUE_11:
		case OP_GET_UPVALUE_12:
		case OP_GET_UPVALUE_13:
		case OP_GET_UPVALUE_14:
		case OP_GET_UPVALUE_15:
		case OP_GET_UPVALUE_16:
		case OP_GET_UPVALUE_17:
		case OP_GET_UPVALUE_18:
		case OP_GET_UPVALUE_19:
		case OP_GET_UPVALUE_20: 
			return localInstruction("OP_GET_UPVALUE_", ins, OP_GET_UPVALUE_0, offset);

		case OP_SET_UPVALUE_0:
		case OP_SET_UPVALUE_1:
		case OP_SET_UPVALUE_2:
		case OP_SET_UPVALUE_3:
		case OP_SET_UPVALUE_4:
		case OP_SET_UPVALUE_5:
		case OP_SET_UPVALUE_6:
		case OP_SET_UPVALUE_7:
		case OP_SET_UPVALUE_8:
		case OP_SET_UPVALUE_9:
		case OP_SET_UPVALUE_10:
		case OP_SET_UPVALUE_11:
		case OP_SET_UPVALUE_12:
		case OP_SET_UPVALUE_13:
		case OP_SET_UPVALUE_14:
		case OP_SET_UPVALUE_15:
		case OP_SET_UPVALUE_16:
		case OP_SET_UPVALUE_17:
		case OP_SET_UPVALUE_18:
		case OP_SET_UPVALUE_19:
		case OP_SET_UPVALUE_20: 
			return localInstruction("OP_SET_UPVALUE_", ins, OP_SET_UPVALUE_0, offset);

        case OP_INVOKE: 
            return invokeInstruction("OP_INVOKE", chunk, offset);

		case OP_RETURN: 
			return simpleInstruction("OP_RETURN", offset);
	}

	fprintf(stderr, "[Error][Debug][Line %d]: Unknown opcode %hhu!\n", line, ins);

	return offset + 1u;
}
