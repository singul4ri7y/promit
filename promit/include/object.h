#ifndef _promit_object_
#define _promit_object_

#pragma once

#include "memory.h"
#include "chunk.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_NATIVE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_DICTIONARY,
    OBJ_LIST,
    OBJ_FILE,
    OBJ_BYTELIST
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    Obj* next;
};

struct ObjString {
    Obj obj;
    bool heapAllocated;
    int length;
    char* buffer;
    uint32_t hash;
};

struct ObjFunction {
    Obj obj;
    uint32_t arity;    // The number of parameters.
    Chunk chunk;
    int upvalueCount;
    ObjString* name;

    /* The module where the function belong. */
    const char* module;
};

struct ObjUpvalue {
    Obj obj;
    Value closed;
    bool isConst;
    struct ObjUpvalue* next;
    Value* location;
};

struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
};

typedef struct {
    Value value;
    bool hadError;
} NativePack;

typedef NativePack (*NativeFn)(VM*, int, Value*);

struct ObjNative {
    Obj obj;
    NativeFn function;
};

struct ObjClass {
    Obj obj;
    ObjString* name;
    Table methods;
    Table statics;
    struct ObjClass* superClass;
};

struct ObjInstance {
    Obj obj;
    ObjClass* klass;
    Table fields;
};

struct ObjBoundMethod {
    Obj obj;
    Value receiver;
    
    // Can be a function. Can be a closure if atleast one upvalue is defined.
    
    Obj* function;
};

struct ObjDictionary {
    Obj obj;
    Table fields;
};

struct ObjList {
    Obj obj;
    int count;
    int capacity;
    Value* values;
};

struct ObjFile {
    Obj obj;
    FILE* file;
};

struct ObjByteList {
    Obj obj;
    size_t size;
    uint8_t* bytes;
};

#define OBJ_TYPE(value) (VALUE_OBJECT(value) -> type)

#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_MEHTOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_DICTIONARY(value)   isObjType(value, OBJ_DICTIONARY)
#define IS_LIST(value)         isObjType(value, OBJ_LIST)
#define IS_FILE(value)         isObjType(value, OBJ_FILE)
#define IS_BYTELIST(value)     isObjType(value, OBJ_BYTELIST)

#define VALUE_STRING(value)  ((ObjString*) VALUE_OBJECT(value))
#define VALUE_CSTRING(value) (VALUE_STRING(value) -> buffer)

#define VALUE_FUNCTION(value)     ((ObjFunction*) VALUE_OBJECT(value))
#define VALUE_CLOSURE(value)      ((ObjClosure*) VALUE_OBJECT(value))
#define VALUE_NATIVE(value)       ((ObjNative*) VALUE_OBJECT(value))
#define VALUE_CLASS(value)        ((ObjClass*) VALUE_OBJECT(value))
#define VALUE_INSTANCE(value)     ((ObjInstance*) VALUE_OBJECT(value))
#define VALUE_BOUND_METHOD(value) ((ObjBoundMethod*) VALUE_OBJECT(value))
#define VALUE_DICTIONARY(value)   ((ObjDictionary*) VALUE_OBJECT(value))
#define VALUE_LIST(value)         ((ObjList*) VALUE_OBJECT(value))
#define VALUE_FILE(value)         ((ObjFile*) VALUE_OBJECT(value))
#define VALUE_BYTELIST(value)     ((ObjByteList*) VALUE_OBJECT(value))

bool isObjType(Value, ObjType);
ObjString* takeString(VM*, char*, size_t, bool);
ObjString* copyString(VM*, const char*, size_t);
ObjFunction* newFunction(VM*);
ObjClosure* newClosure(VM*, ObjFunction*);
ObjNative* newNative(VM*, NativeFn);
ObjUpvalue* newUpvalue(VM*, Value*, bool);
ObjClass* newClass(VM*, ObjString*);
ObjInstance* newInstance(VM*, ObjClass*);
ObjBoundMethod* newBoundMethod(VM*, Value, Obj*);
ObjDictionary* newDictionary(VM*);
ObjList* newList(VM*);
ObjFile* newFile(VM*);
ObjByteList* newByteList(VM*);
bool printObject(const Value*);
void printObjectRaw(const Value*);

#endif
