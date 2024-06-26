#ifndef _promit_vm_
#define _promit_vm_

#pragma once

#include "table.h"
#include "value.h"

#define FRAMES_MAX 256
#define STACK_MAX (FRAMES_MAX * 256)

#define MAX_SAFE_INTEGER 9007199254740991
#define MIN_SAFE_INTEGER -9007199254740992

#define TAKE_STRING(buffer, length, heapAllocated) takeString(vm, buffer, length, heapAllocated)

#define NATIVE_R_ERR(format, ...) RUNTIME_ERROR(format, ##__VA_ARGS__);\
    pack.hadError = true;\
    return pack;

typedef struct {
    Obj* function;
    uint8_t* ip;
    int slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    short frameCount;

    // The version string.
    const char* version;

    short includeDepth;
    
    Value stack[STACK_MAX];
    int stackTop;
    
    Table globals;
    Table strings;
    Table modules;

    Obj* objects;

    int grayCount;
    int grayCapacity;
    Obj** grayStack;

    size_t bytesAllocated;
    size_t nextGC;
    
    ObjUpvalue* openUpvalues;
    
    ObjString* initString;
    ObjString* includePath;
    
    bool inREPL;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILATION_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct NumberData {
    bool hadError;
    double number;
} NumberData;

typedef struct BooleanData {
    bool hadError;
    bool boolean;
} BooleanData;

bool callValue(VM*, Value, uint8_t);

void initVM(VM*); 
void freeVM(VM*);
InterpretResult run(VM*);
InterpretResult interpret(VM*, const char*, const char*);
void runtimeError(VM*, const char*, ...);

void resetStack(VM*);

void stack_push(VM*, Value);
Value stack_pop(VM*);

bool valuesEqual(const Value, const Value);
BooleanData toBoolean(VM*, Value*);

int __printf(const char*, ...);
NumberData toNumber(VM*, Value* value);

#endif
