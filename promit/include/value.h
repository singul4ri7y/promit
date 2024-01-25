#ifndef _promit_value_
#define _promit_value_

#pragma once

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;
typedef struct ObjClosure ObjClosure;
typedef struct ObjNative ObjNative;
typedef struct ObjUpvalue ObjUpvalue;
typedef struct ObjClass ObjClass;
typedef struct ObjInstance ObjInstance;
typedef struct ObjBoundMethod ObjBoundMethod;
typedef struct ObjDictionary ObjDictionary;
typedef struct ObjList ObjList;
typedef struct ObjFile ObjFile;
typedef struct ObjByteList ObjByteList;

typedef enum {
    VAL_NUMBER,
    VAL_BOOLEAN,
    VAL_NULL,
    VAL_OBJECT
} ValueType;

typedef struct {
    ValueType type;

    union {
        double number;
        bool boolean;
        Obj* obj;
    } as;
} Value;

#define BOOL_VAL(value)   ((Value) { VAL_BOOLEAN, { .boolean = value } })
#define NULL_VAL          ((Value) { VAL_NULL, { .number = 0 } })
#define NUMBER_VAL(value) ((Value) { VAL_NUMBER, { .number = value } })
#define OBJECT_VAL(value) ((Value) { VAL_OBJECT, { .obj = (Obj*) value } })

#define VALUE_BOOL(value)   ((value).as.boolean)
#define VALUE_NUMBER(value) ((value).as.number)
#define VALUE_OBJECT(value) ((value).as.obj)

#define IS_BOOL(value)   ((value).type == VAL_BOOLEAN)
#define IS_NULL(value)   ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

#define IS_NAN(value) isnan((value).as.number)

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

typedef struct {
    Value value;
    bool isConst;
} ValueContainer;

void   initValueArray(ValueArray*);
int    writeValueArray(ValueArray*, Value);
void   freeValueArray(ValueArray*);
bool   printValue(const Value*);
void   printValueRaw(const Value*);

#endif
