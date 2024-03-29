#ifndef _promit_chunk_
#define _promit_chunk_

#pragma once

#include "table.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_LONG,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_LONG,
    
    OP_SET_LOCAL,
    OP_SET_LOCAL_LONG,
    
    OP_SET_LOCAL_0,
    OP_SET_LOCAL_1,
    OP_SET_LOCAL_2,
    OP_SET_LOCAL_3,
    OP_SET_LOCAL_4,
    OP_SET_LOCAL_5,
    OP_SET_LOCAL_6,
    OP_SET_LOCAL_7,
    OP_SET_LOCAL_8,
    OP_SET_LOCAL_9,
    OP_SET_LOCAL_10,
    OP_SET_LOCAL_11,
    OP_SET_LOCAL_12,
    OP_SET_LOCAL_13,
    OP_SET_LOCAL_14,
    OP_SET_LOCAL_15,
    OP_SET_LOCAL_16,
    OP_SET_LOCAL_17,
    OP_SET_LOCAL_18,
    OP_SET_LOCAL_19,
    OP_SET_LOCAL_20,
    
    OP_GET_LOCAL,
    OP_GET_LOCAL_LONG,
    
    OP_GET_LOCAL_0,
    OP_GET_LOCAL_1,
    OP_GET_LOCAL_2,
    OP_GET_LOCAL_3,
    OP_GET_LOCAL_4,
    OP_GET_LOCAL_5,
    OP_GET_LOCAL_6,
    OP_GET_LOCAL_7,
    OP_GET_LOCAL_8,
    OP_GET_LOCAL_9,
    OP_GET_LOCAL_10,
    OP_GET_LOCAL_11,
    OP_GET_LOCAL_12,
    OP_GET_LOCAL_13,
    OP_GET_LOCAL_14,
    OP_GET_LOCAL_15,
    OP_GET_LOCAL_16,
    OP_GET_LOCAL_17,
    OP_GET_LOCAL_18,
    OP_GET_LOCAL_19,
    OP_GET_LOCAL_20,
    
    OP_TRUE,
    OP_FALSE,
    OP_NULL,
    OP_NOT,
    OP_INFINITY,
    OP_INSTOF,
    OP_NAN,
    OP_NEGATE,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_ADD,
    OP_SUBSTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULUS,
    OP_BITWISE_NEGATION,
    OP_XOR,
    OP_SHOW,
    OP_SHOWL,
    OP_RAW_SHOW,
    OP_RECEIVE,
    OP_POP,
    OP_SILENT_POP,
    
    OP_TYPEOF,
    
    OP_BITWISE_AND,
    OP_BITWISE_OR,
    OP_RIGHT_SHIFT,
    OP_LEFT_SHIFT,
    
    OP_JUMP_IF_FALSE,
    OP_JUMP_OPR,
    OP_JUMP,
    OP_LOOP,
    
    OP_CALL,
    
    OP_CALL_0,
    OP_CALL_1,
    OP_CALL_2,
    OP_CALL_3,
    OP_CALL_4,
    OP_CALL_5,
    OP_CALL_6,
    OP_CALL_7,
    OP_CALL_8,
    OP_CALL_9,
    OP_CALL_10,
    OP_CALL_11,
    OP_CALL_12,
    OP_CALL_13,
    OP_CALL_14,
    OP_CALL_15,
    OP_CALL_16,
    OP_CALL_17,
    OP_CALL_18,
    OP_CALL_19,
    OP_CALL_20,
    
    OP_CLOSURE,
    OP_CLOSURE_LONG,
    
    OP_GET_UPVALUE,
    OP_GET_UPVALUE_LONG,
    
    OP_GET_UPVALUE_0,
    OP_GET_UPVALUE_1,
    OP_GET_UPVALUE_2,
    OP_GET_UPVALUE_3,
    OP_GET_UPVALUE_4,
    OP_GET_UPVALUE_5,
    OP_GET_UPVALUE_6,
    OP_GET_UPVALUE_7,
    OP_GET_UPVALUE_8,
    OP_GET_UPVALUE_9,
    OP_GET_UPVALUE_10,
    OP_GET_UPVALUE_11,
    OP_GET_UPVALUE_12,
    OP_GET_UPVALUE_13,
    OP_GET_UPVALUE_14,
    OP_GET_UPVALUE_15,
    OP_GET_UPVALUE_16,
    OP_GET_UPVALUE_17,
    OP_GET_UPVALUE_18,
    OP_GET_UPVALUE_19,
    OP_GET_UPVALUE_20,
    
    OP_SET_UPVALUE,
    OP_SET_UPVALUE_LONG,
    
    OP_SET_UPVALUE_0,
    OP_SET_UPVALUE_1,
    OP_SET_UPVALUE_2,
    OP_SET_UPVALUE_3,
    OP_SET_UPVALUE_4,
    OP_SET_UPVALUE_5,
    OP_SET_UPVALUE_6,
    OP_SET_UPVALUE_7,
    OP_SET_UPVALUE_8,
    OP_SET_UPVALUE_9,
    OP_SET_UPVALUE_10,
    OP_SET_UPVALUE_11,
    OP_SET_UPVALUE_12,
    OP_SET_UPVALUE_13,
    OP_SET_UPVALUE_14,
    OP_SET_UPVALUE_15,
    OP_SET_UPVALUE_16,
    OP_SET_UPVALUE_17,
    OP_SET_UPVALUE_18,
    OP_SET_UPVALUE_19,
    OP_SET_UPVALUE_20,
    
    OP_CLOSE_UPVALUE,
    
    OP_POST_INCREMENT,
    OP_POST_DECREMENT,
    OP_PRE_INCREMENT,
    OP_PRE_DECREMENT,

    OP_CLASS,
    OP_CLASS_LONG,
    OP_INHERIT,
    OP_DNM_SUPER,
    OP_SUPER,
    OP_SUPER_LONG,
    OP_SUPER_INVOKE,
    OP_DNM_SUPER_INVOKE,
    OP_SUPER_INVOKE_LONG,
    OP_SET_PROPERTY,
    OP_SET_PROPERTY_LONG,
    OP_GET_PROPERTY,
    OP_GET_PROPERTY_LONG,
    OP_DELETE_PROPERTY,
    OP_DELETE_PROPERTY_LONG,
    OP_GET_PROPERTY_INST,
    OP_GET_PROPERTY_INST_LONG,
    
    OP_SET_STATIC,
    OP_SET_STATIC_LONG,
    OP_GET_STATIC_PROPERTY,
    OP_GET_STATIC_PROPERTY_LONG,
    OP_GET_STATIC_PROPERTY_INST,
    OP_GET_STATIC_PROPERTY_INST_LONG,      // Yeah, this sucks!
    OP_SET_STATIC_PROPERTY,
    OP_SET_STATIC_PROPERTY_LONG,
    
    // Dynamic.
    
    OP_DNM_GET_PROPERTY,
    OP_DNM_GET_PROPERTY_INST,
    OP_DNM_SET_PROPERTY,
    OP_DNM_DELETE_PROPERTY,
    
    OP_METHOD,
    OP_METHOD_LONG,
    
    OP_INVOKE,
    OP_INVOKE_LONG,
    OP_DNM_INVOKE_START,
    OP_DNM_INVOKE_END,
    
    OP_DICTIONARY,
    OP_ADD_DICTIONARY,
    OP_ADD_DICTIONARY_LONG,
    
    OP_LIST,
    OP_ADD_LIST,
    OP_ADD_LIST_LONG,
    
    OP_DUP,
    
    OP_RETURN
} OpCode;

typedef struct {
    int offset;
    int line;
} Line;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t* code;
    ValueArray constants;
    int lineSize;
    int highestLine;
    Line* lines;
} Chunk;

void   initChunk(Chunk*);
void   writeChunk(Chunk*, uint8_t, int);
void   writeConstant(Chunk*, Value, int);

size_t addConstant(Chunk*, Value);
void   freeChunk(Chunk*);
int    getLine(Chunk*, int);

#endif
