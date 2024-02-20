#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "object.h"
#include "compiler.h"
#include "memory.h"
#include "lib.h"

#ifdef DEBUG
#include "debug.h"
#endif

#define PUSH(value) stack_push(vm, (value))
#define POP() stack_pop(vm)

static void defineNative(VM*, const char*, NativeFn);

static NativePack author(VM* vm, int argCount, Value* values) {
    NativePack pack;

    pack.hadError = false;
    pack.value    = OBJECT_VAL(TAKE_STRING("SD Asif Hossein", 15u, false));

    return pack;
}

static NativePack include(VM* vm, int argCount, Value* args) {
    NativePack pack;

    pack.hadError = false;
    pack.value    = NULL_VAL;

    if(argCount < 2) 
        return pack;

    if(!IS_STRING(args[1])) {
        NATIVE_R_ERR("Expected the first argument to be a string in include(promit_file)!");
    }

    ObjString* string = VALUE_STRING(args[1]);

    if(string -> length == 0) {
        NATIVE_R_ERR("Expected a full file name/path in include(promit_file)!");
    }

    if(string -> length >= 2 && string -> buffer[0] == '.' && string -> buffer[1] == '.') {
        NATIVE_R_ERR("Including files directly from parent directory is forbidden! Try using absolute path.");
    }

    char* inc_path = NULL;
    size_t inc_siz = 0;

    if(vm -> includePath != NULL) {
        inc_path = vm -> includePath -> buffer;
        inc_siz  = vm -> includePath -> length;
    }

    char* filepath = malloc((string -> length + 1) * sizeof(char));

    strcpy(filepath, string -> buffer);

    char* alternate_path = NULL;

    if(string -> length < 7 || strcmp(string -> buffer + string -> length - 7, ".promit")) {
        for(int i = 0; i < string -> length; i++) {
            if(filepath[i] == '.') 
                filepath[i] = '/';
        }

        alternate_path = malloc((string -> length + 13) * sizeof(char));

        strcpy(alternate_path, filepath);

        filepath = realloc(filepath, (string -> length + 8) * sizeof(char));

        /* Just to be sure. */
        strcpy(filepath, alternate_path);

        strcpy(alternate_path + string -> length, "/main.promit");
        strcpy(filepath + string -> length, ".promit");
    } else {
        for(int i = string -> length - 8; i >= 0; i--) {
            if(filepath[i] == '.') 
                filepath[i] = '/';
        }
    }

    char* buffer = NULL;

    if(inc_path != NULL) {
        buffer = malloc((strlen(filepath) + inc_siz + 1) * sizeof(char));

        strcpy(buffer, inc_path);
        strcpy(buffer + inc_siz, filepath);

        free(filepath);
        filepath = buffer;
        
        if(alternate_path != NULL) {
            buffer = malloc((strlen(alternate_path) + inc_siz + 1) * sizeof(char));

            strcpy(buffer, inc_path);
            strcpy(buffer + inc_siz, alternate_path);

            free(alternate_path);
            alternate_path = buffer;
        }
    }

    FILE* file = fopen(filepath, "rb");

    // Try alternate_path if filepath does not exist.

    if(file == NULL) {
        if(alternate_path != NULL) {
            file = fopen(alternate_path, "rb");

            if(file == NULL) {
                free(filepath);
                free(alternate_path);

                NATIVE_R_ERR("Failed to include module '%s'!", string -> buffer);
            }
        } else { NATIVE_R_ERR("Failed to include module '%s'!", string -> buffer); }
    }

    fseek(file, 0, SEEK_END);

    long size = ftell(file);

    rewind(file);

    buffer = ALLOCATE(char, size + 1);

    if(buffer == NULL) {
        free(filepath);

        if(alternate_path != NULL) 
            free(alternate_path);
        
        fclose(file);

        NATIVE_R_ERR("Could not allocate memory for file buffer!", string -> buffer);

        fprintf(stderr, "\nFailed to inlcude file '%s'!\n", string -> buffer);
    }

    fread(buffer, size, sizeof(char), file);

    buffer[size] = 0;

    // Check whether the module has already been included or not.
    
    ObjString* content = TAKE_STRING(buffer, strlen(buffer), true);

    // Save the 'content' from GC.

    PUSH(OBJECT_VAL(content));

    ValueContainer valueContainer;

    if(tableGet(&vm -> modules, content, &valueContainer)) {
        // The module exists. Now return the value.
        pack.value = valueContainer.value;

        goto out;
    }

    /* NOW LOAD AND ADD THE MODULE TO THE MODULES TABLE. */

    // Update include depth.

    vm -> includeDepth++;

    // Turn off REPL mode if enabled.

    bool repl = vm -> inREPL;

    vm -> inREPL = false;

    InterpretResult result = interpret(vm, buffer, true);

    vm -> inREPL = repl;

    vm -> includeDepth--;

    if(result != INTERPRET_OK) {
        if(vm -> includeDepth == 0) 
            fprintf(stderr, "\nFailed to include file '%s' due to error!\n", string -> buffer);
        
        pack.hadError = true;

        goto out;
    }

    pack.value = POP();

    tableInsert(&vm -> modules, content, (ValueContainer) { pack.value, true });

    POP();    // The saved 'content'.

out: 
    free(filepath);

    if(alternate_path != NULL) 
        free(alternate_path);

    fclose(file);

    return pack;
}

// Specific assert function for Promit.

static NativePack promitAssert(VM* vm, int argCount, Value* values) {
    NativePack pack;

    pack.hadError = false;
    pack.value    = NULL_VAL;

    if(argCount < 2) {
        NATIVE_R_ERR("Too few arguments to call assert(bool)!");
    }

    BooleanData data = toBoolean(vm, values + 1u);

    if(data.hadError == true) {
        pack.hadError = true;

        return pack;
    }

    if(data.boolean == false) {
        NATIVE_R_ERR("Promit assertion failed!");
    }

    pack.value = BOOL_VAL(true);

    return pack;
}

// Wrapper classes.

ObjClass* vmNumberClass;
ObjClass* vmListClass;
ObjClass* vmStringClass;
ObjClass* vmDictionaryClass;
ObjClass* vmFunctionClass;

// Non-wrapper classes.

ObjString* timeClass;

// Field locations from lib.c

extern ObjString* listField;
extern ObjString* stringField;
extern ObjString* dictionaryField;
extern ObjString* numberField;

static NativePack len(VM* vm, int argCount, Value* args) {
    NativePack pack;
    
    pack.hadError = false;
    
    if(argCount < 2) {
        NATIVE_R_ERR("Expected an argument in function 'len(string | dictionary | list)'!");
    }
    
    double size = 0;
    
    if(IS_STRING(args[1])) 
        size = VALUE_STRING(args[1]) -> length;
    else if(IS_DICTIONARY(args[1])) 
        size = VALUE_DICTIONARY(args[1]) -> fields.count;
    else if(IS_LIST(args[1])) 
        size = VALUE_LIST(args[1]) -> count;
    
    // The wrapper instances.
    else if(IS_INSTANCE(args[1])) {
        ObjInstance* instance = VALUE_INSTANCE(args[1]);

        ValueContainer container;

        if(instance -> klass == vmListClass) {
            tableGet(&VALUE_INSTANCE(args[1]) -> fields, listField, &container);

            size = VALUE_LIST(container.value) -> count;
        }
        else if(instance -> klass == vmStringClass) {
            tableGet(&VALUE_INSTANCE(args[1]) -> fields, stringField, &container);

            size = VALUE_STRING(container.value) -> length;
        } 
        else if(instance -> klass == vmDictionaryClass) {
            tableGet(&VALUE_INSTANCE(args[1]) -> fields, dictionaryField, &container);

            size = VALUE_DICTIONARY(container.value) -> fields.count;
        } else {
            NATIVE_R_ERR("Invalid argument in function 'len(string | dictionary | list | wrapper-instance)'!");
        }
    } else {
        NATIVE_R_ERR("Invalid argument in function 'len(string | dictionary | list | wrapper-instance)'!");
    }
    
    pack.value = NUMBER_VAL(size);
    
    return pack;
}

static NativePack isConstProp(VM* vm, int argCount, Value* values) {
    NativePack pack;

    pack.hadError = false;
    pack.value    = NULL_VAL;

    if(argCount < 2) {
        NATIVE_R_ERR("Too few arguments to call is_const_prop(value, prop_name)!");
    }

    if(!IS_INSTANCE(values[1]) && !IS_DICTIONARY(values[1]) && !IS_CLASS(values[1])) {
        NATIVE_R_ERR("Provided value is not instance/dictionary or class (static)!");
    }

    if(argCount < 3) {
        pack.value = NUMBER_VAL(NAN);

        return pack;
    }

    ObjString* name = VALUE_STRING(values[2]);

    Table* fields;

    ValueContainer valueContainer;

    if(IS_CLASS(values[1])) {
        fields = &VALUE_CLASS(values[1]) -> statics;

        if(tableGet(fields, name, &valueContainer)) {
            pack.value = BOOL_VAL(valueContainer.isConst);

            return pack;
        } else {
            pack.value = NUMBER_VAL(NAN);

            return pack;
        }
    }
    else if(IS_INSTANCE(values[1])) 
        fields = &VALUE_INSTANCE(values[1]) -> fields;
    else fields = &VALUE_DICTIONARY(values[1]) -> fields;

    if(tableGet(fields, name, &valueContainer)) 
        pack.value = BOOL_VAL(valueContainer.isConst);
    else pack.value = NUMBER_VAL(NAN);

    return pack;
}

// Wrapper status.

typedef struct WrapperStatus {
    bool status;
    bool hadError;
} WrapperStatus;

void gcVMIgnore(VM* vm) {
    markObject((Obj*) vm -> initString);

    if(vm -> includePath != NULL) 
        markObject((Obj*) vm -> includePath);

    // Mark the modules table.
    
    markTable(&vm -> modules);
}

ObjFile* vm_stdin;
ObjFile* vm_stdout;

void initVM(VM* vm) {
    vm -> inREPL      = false;
    vm -> objects     = NULL;
    vm -> openUpvalues = NULL;

    vm -> includeDepth = 0;

    vm -> grayCount    = 0;
    vm -> grayCapacity = 0;
    vm -> grayStack    = NULL;

    vm -> bytesAllocated = 0u;
    vm -> nextGC        = 1048576u;        // 1 Megabyte.
    
    initTable(&vm -> globals);
    initTable(&vm -> strings);
    initTable(&vm -> modules);
    
    setMemoryVM(vm);
    
    vm -> initString  = TAKE_STRING("init", 4u, false);
    vm -> includePath = NULL;
    
    resetStack(vm);
    
    defineNative(vm, "author", author);
    defineNative(vm, "len", len);
    defineNative(vm, "is_const_prop", isConstProp);
    defineNative(vm, "include", include);
    defineNative(vm, "assert", promitAssert);
    
    // Standard libraries.
    
    initFileLib(vm);
    initSystemLib(vm);
    initMathLib(vm);
    initTimeLib(vm);
    initNumberLib(vm);
    initListLib(vm);
    initStringLib(vm);
    initDictionaryLib(vm);
    initFunctionLib(vm);
}

void freeVM(VM* vm) {
    freeObjects(vm);

    // Free the gray stack.
    
    free(vm -> grayStack);
    
    freeTable(&vm -> globals);
    freeTable(&vm -> strings);
    freeTable(&vm -> modules);
}

// Search through the opened upvalues in the vm structure and capture one.
// All the opened upvalues stays in the stack. So, local pointer stays the
// same.

static ObjUpvalue* captureUpvalue(VM* vm, Value* local, bool isConst) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm -> openUpvalues;
    
    if(upvalue != NULL && upvalue -> location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue -> next;
    }
    
    if(upvalue != NULL && upvalue -> location == local) 
        return upvalue;
    
    ObjUpvalue* createdUpvalue = newUpvalue(vm, local, isConst);
    
    createdUpvalue -> next = upvalue;
    
    if(prevUpvalue == NULL) 
        vm -> openUpvalues = createdUpvalue;
    else prevUpvalue -> next = createdUpvalue;
    
    return createdUpvalue;
}

static ObjClosure* getClosure(Obj* callee) {
    return (ObjClosure*) callee;
}

static ObjFunction* getFunction(Obj* callee) {
    if(callee -> type == OBJ_FUNCTION) 
        return (ObjFunction*) callee;
    
    return getClosure(callee) -> function;
}

void runtimeError(VM* vm, const char* format, ...) {
    fprintf(stderr, "[Error][Runtime]: ");

    va_list args;

    va_start(args, format);

    vfprintf(stderr, format, args);

    va_end(args);

    fputs("\n", stderr);

    short count = 0;
    
    for(short i = vm -> frameCount - 1; i >= 0; i--) {
        CallFrame* frame = vm -> frames + i;

        if(count++ < 3) {
            ObjFunction* function = getFunction(frame -> function);
            
            size_t instruction = frame -> ip - function -> chunk.code - 1;
            
            fprintf(stderr, "  [line %d] in ", getLine(&function -> chunk, instruction));
            
            if(function -> name == NULL) 
                fprintf(stderr, "function anonymous()\n");
            else fprintf(stderr, "function %s()\n", function -> name -> buffer);
        } else {
            fprintf(stderr, "\n  ... %hd frames more ...\n\n", vm -> frameCount - count);

            count = 0; i = 1;
        }
    }

    resetStack(vm);
}

static Value* peek(VM* vm, int distance) {
    return vm -> stack + (vm -> stackTop - 1 - distance);
}

static bool call(VM* vm, Obj* callee, uint8_t argCount) {
    ObjFunction* function = getFunction(callee);
    
    if(argCount > function -> arity) 
        vm -> stackTop -= argCount - function -> arity;
    else if(argCount < function -> arity) {
        int i = function -> arity - argCount;
        
        while(i--) 
            PUSH(NULL_VAL);
    }
    
    if(vm -> frameCount == FRAMES_MAX) {
        RUNTIME_ERROR("Stack overflow! Maximum call frames exceeded!");
        return false;
    }
    
    CallFrame* frame = &vm -> frames[vm -> frameCount++];
    
    frame -> function = (Obj*) callee;
    frame -> ip      = function -> chunk.code;
    frame -> slots    = vm -> stackTop - function -> arity - 1u;
    
    return true;
}

bool callValue(VM* vm, Value callee, uint8_t argCount) {
    if(IS_OBJECT(callee)) {
        switch(OBJ_TYPE(callee)) {
            case OBJ_FUNCTION: 
                return call(vm, (Obj*) VALUE_FUNCTION(callee), argCount);
                
            case OBJ_CLOSURE: 
                return call(vm, (Obj*) VALUE_CLOSURE(callee), argCount);
                
            case OBJ_NATIVE: {
                ObjNative* native = VALUE_NATIVE(callee);
                
                NativeFn function = native -> function;

                // Save the previous stack location, so that it can be restored to 
                // pop out the pushed stack values if something goes wrong.

                int current_stack = vm -> stackTop;
                
                NativePack result = function(vm, argCount + 1, vm -> stack + vm -> stackTop - argCount - 1u);

                vm -> stackTop = current_stack;
                
                if(!result.hadError) {
                    vm -> stackTop -= argCount + 1u;
                
                    PUSH(result.value);
                }
                
                return !result.hadError;
            }
            
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* boundMethod = VALUE_BOUND_METHOD(callee);
                
                // The function can be native or closure or just a funciton.
                
                vm -> stack[vm -> stackTop - argCount - 1u] = boundMethod -> receiver;
                
                return callValue(vm, OBJECT_VAL(boundMethod -> function), argCount);
            }
            
            case OBJ_CLASS: {
                ObjClass* klass = VALUE_CLASS(callee);

                vm -> stack[vm -> stackTop - argCount - 1u] = OBJECT_VAL(newInstance(vm, klass));
                
                ValueContainer initializer;
                
                if(tableGet(&klass -> methods, vm -> initString, &initializer)) {
                    return callValue(vm, initializer.value, argCount);
                }
                
                return true;
            }
            
            default: 
                break;
        }
    }
    
    RUNTIME_ERROR("Can only call function and classes!");
    
    return false;
}

static void defineNative(VM* vm, const char* name, NativeFn native) {
    PUSH(OBJECT_VAL(takeString(vm, (char*) name, strlen(name), false)));
    PUSH(OBJECT_VAL(newNative(vm, native)));
    
    ValueContainer valueContainer;
    
    valueContainer.isConst = true;
    valueContainer.value   = vm -> stack[1];
    
    tableInsert(&vm -> globals, VALUE_STRING(vm -> stack[0]), valueContainer);
    
    POP();
    POP();
}

bool valuesEqual(const Value value1, const Value value2) {
    // Wrapper class exception only with String class and primitive string.

    if((IS_INSTANCE(value1) || IS_INSTANCE(value2)) && (IS_STRING(value1) || IS_STRING(value2))) {
        ObjString* string1;
        ObjString* string2;

        if(IS_STRING(value1) && IS_INSTANCE(value2) && VALUE_INSTANCE(value2) -> klass == vmStringClass) {
            string1 = VALUE_STRING(value1);

            ValueContainer stringContainer;

            tableGet(&VALUE_INSTANCE(value2) -> fields, stringField, &stringContainer);

            string2 = VALUE_STRING(stringContainer.value);

            return string1 -> length == string2 -> length && string1 -> hash == string2 -> hash &&
                !memcmp(string1 -> buffer, string2 -> buffer, string1 -> length * sizeof(char));
        }
        else if(IS_STRING(value2) && IS_INSTANCE(value1) && VALUE_INSTANCE(value1) -> klass == vmStringClass) {
            string2 = VALUE_STRING(value2);

            ValueContainer stringContainer;

            tableGet(&VALUE_INSTANCE(value1) -> fields, stringField, &stringContainer);

            string1 = VALUE_STRING(stringContainer.value);

            return string1 -> length == string2 -> length && string1 -> hash == string2 -> hash &&
                !memcmp(string1 -> buffer, string2 -> buffer, string1 -> length * sizeof(char));
        }
    }
    else if((IS_INSTANCE(value1) || IS_INSTANCE(value2)) && (IS_NUMBER(value1) || IS_NUMBER(value2))) {
        double a, b;

        if(IS_NUMBER(value1) && IS_INSTANCE(value2) && VALUE_INSTANCE(value2) -> klass == vmNumberClass) {
            a = VALUE_NUMBER(value1);

            ValueContainer numberContainer;

            tableGet(&VALUE_INSTANCE(value2) -> fields, numberField, &numberContainer);

            b = VALUE_NUMBER(numberContainer.value);

            return a == b;
        }
        else if(IS_NUMBER(value2) && IS_INSTANCE(value1) && VALUE_INSTANCE(value1) -> klass == vmNumberClass) {
            b = VALUE_NUMBER(value2);

            ValueContainer numberContainer;

            tableGet(&VALUE_INSTANCE(value1) -> fields, numberField, &numberContainer);

            a = VALUE_NUMBER(numberContainer.value);

            return a == b;
        }
    }

    if(value1.type != value2.type) 
        return false;
    
    switch(value1.type) {
        case VAL_BOOLEAN: return VALUE_BOOL(value1) == VALUE_BOOL(value2);
        case VAL_NULL:    return true;
        case VAL_NUMBER:  return VALUE_NUMBER(value1) == VALUE_NUMBER(value2);
        case VAL_OBJECT:  return VALUE_OBJECT(value1) == VALUE_OBJECT(value2);
    }
    
    return false;
}

#define READ_BYTE() *frame -> ip++

#define VALUE_COMP(op) {\
    Value value2 = POP(),\
         value1 = POP();\
    vmNumberData data1 = vmToNumber(vm, &value1);\
    vmNumberData data2 = vmToNumber(vm, &value2);\
    if(!data1.isRepresentable || !data2.isRepresentable) {\
        RUNTIME_ERROR("Operand must be comparable!");\
        return INTERPRET_RUNTIME_ERROR;\
    }\
    PUSH(BOOL_VAL(data1.number op data2.number));\
    break;\
}

static char* concat(VM* vm, char* str1, char* str2) {
    size_t len1 = strlen(str1), len2 = strlen(str2);

    size_t length = len1 + len2;

    char* result = ALLOCATE(char, length + 1u);

    memcpy(result, str1, len1 * sizeof(char));
    memcpy(result + len1, str2, len2 * sizeof(char));

    result[length] = 0;

    return result;
}

BooleanData toBoolean(VM* vm, Value* value) {
    BooleanData data;

    data.hadError = false;
    data.boolean  = false;
                
    if(IS_NUMBER(*value)) {
        if(!IS_NAN(*value)) 
            data.boolean = !!value -> as.number;
        else data.boolean = false;
    }
    else if(IS_BOOL(*value)) 
        data.boolean = value -> as.boolean;
    else if(IS_NULL(*value)) 
        data.boolean = false;
    else if(IS_OBJECT(*value)) {
        switch(OBJ_TYPE(*value)) {
            case OBJ_STRING: {
                ObjString* string = VALUE_STRING(*value);
                
                data.boolean = !!string -> length;
                
                break;
            }
            
            case OBJ_LIST: {
                ObjList* list = VALUE_LIST(*value);
                
                data.boolean = !!list -> count;
                
                break;
            }
            
            case OBJ_BYTELIST: {
                ObjByteList* byteList = VALUE_BYTELIST(*value);
                
                data.boolean = !!byteList -> size;
                
                break;
            }
            
            case OBJ_FILE: {
                ObjFile* file = VALUE_FILE(*value);
                
                data.boolean = file -> file != NULL;
                
                break;
            }

            // For wrapper classes.

            case OBJ_INSTANCE: {
                ObjInstance* instance = VALUE_INSTANCE(*value);

                ObjClass* klass = instance -> klass;

                // For number instance.

                ValueContainer container;

                if(klass == vmNumberClass) {
                    tableGet(&instance -> fields, numberField, &container);

                    if(!IS_NAN(container.value)) 
                        data.boolean = !!container.value.as.number;
                    else data.boolean = false;
                }
                else if(klass == vmListClass) {
                    tableGet(&instance -> fields, listField, &container);

                    data.boolean = !!VALUE_LIST(container.value) -> count;
                } 
                else if(klass == vmStringClass) {
                    tableGet(&instance -> fields, stringField, &container);

                    data.boolean = !!VALUE_STRING(container.value) -> length;
                } else {
                    ObjInstance* instance = VALUE_INSTANCE(*value);

                    ValueContainer represent;

                    ObjString* name = takeString(vm, "__represent__", 13u, false);

                    bool found = false;

                    if((found = tableGet(&instance -> fields, name, &represent))) {}
                    else if((found = tableGet(&instance -> klass -> methods, name, &represent))) {}

                    if(found) {
                        Value callable = represent.value;

                        if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
                            stack_push(vm, callable);

                            if(callValue(vm, callable, 0u)) {
                                vm -> stack[vm -> stackTop - 1u] = *value;

                                if(run(vm) != INTERPRET_RUNTIME_ERROR) {
                                    callable = stack_pop(vm);
                                    
                                    return toBoolean(vm, &callable);
                                }
                            }

                            data.hadError = true;
                            
                            return data;
                        }
                        else if(IS_NATIVE(callable)) {
                            NativeFn native = VALUE_NATIVE(callable) -> function;

                            NativePack pack = native(vm, 1, (Value*) value);

                            if(!pack.hadError) 
                                return toBoolean(vm, &pack.value);
                            
                            data.hadError = pack.hadError;
                            
                            return data;
                        }
                    }

                    data.boolean = true;
                }

                break;
            }

            case OBJ_DICTIONARY: {
                Table* fields = &VALUE_DICTIONARY(*value) -> fields;

                ObjString* represent = TAKE_STRING("__represent__", 13u, false);

                ValueContainer valueContainer;
                
                if(tableGet(fields, represent, &valueContainer)) {
                    Value callable = valueContainer.value;

                    if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
                        stack_push(vm, callable);

                        uint8_t arity = IS_FUNCTION(callable) ? VALUE_FUNCTION(callable) -> arity :
                                        VALUE_CLOSURE(callable) -> function -> arity;
                        
                        if(arity >= 1) {
                            stack_push(vm, *value);

                            for(uint8_t i = 0u; i < arity - 1; i++) 
                                stack_push(vm, NULL_VAL);
                        }

                        if(callValue(vm, callable, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
                            // Reuse the variable.

                            callable = stack_pop(vm);

                            return toBoolean(vm, &callable);
                        }
                        
                        data.hadError = true;
                        
                        return data;
                    }
                    else if(IS_NATIVE(callable)) {
                        NativeFn native = VALUE_NATIVE(callable) -> function;

                        NativePack pack = native(vm, 1, (Value*) value);

                        if(!pack.hadError) 
                            return toBoolean(vm, &pack.value);
                        
                        data.hadError = pack.hadError;
                        
                        return data;
                    }
                }

                // Fallthrough.
            }
            
            default: 
                data.boolean = true;
                break;
        }
    }
                
    return data;
}

static uint32_t readBytes(CallFrame* frame, bool isLong) {
    if(!isLong) return READ_BYTE();
    
    size_t constant = READ_BYTE();
    
    constant <<= 0x8;

    constant |= READ_BYTE();

    constant <<= 0x8;

    constant |= READ_BYTE();

    return constant;
}

static Value readConstant(CallFrame* frame, ObjFunction* function, bool isLong) {
    return *(function -> chunk.constants.values + readBytes(frame, isLong));
}

static int istrcmp(const char* a, const char* b) {
    int d;
    
    for(;; a++, b++) {
        d = tolower((unsigned char) *a) - tolower((unsigned char) *b);
        
        if(d != 0 || !*a) 
            return d;
    }
}

static void defineMethod(VM* vm, ObjString* name, bool isConst, bool isStatic) {
    ObjClass* klass = VALUE_CLASS(*peek(vm, 1));
    
    ValueContainer valueContainer;
    
    valueContainer.isConst = isConst;
    valueContainer.value   = *peek(vm, 0);
    
    Table* fields;
    
    if(isStatic) 
        fields = &klass -> statics;
    else fields = &klass -> methods;
    
    tableInsert(fields, name, valueContainer);
    
    POP();
}

static void closeUpvalues(VM* vm, Value* last) {
    while(vm -> openUpvalues != NULL && vm -> openUpvalues -> location >= last) {
        ObjUpvalue* upvalue = vm -> openUpvalues;
        
        upvalue -> closed   = *upvalue -> location;
        upvalue -> location = &upvalue -> closed;
        
        vm -> openUpvalues  = upvalue -> next;
    }
}

static void pop_state(VM* vm, uint8_t state) {
    switch(state) {
        case 0: {
            POP();    // Instance.
            
            break;
        }
        
        case 1: {
            POP();    // Expression generated value.
            POP();    // Instance.
            
            break;
        }
        
        case 2:
        case 3: {
            // Do nothing.
            
            break;
        }
    }
}

static bool bindMethod(VM* vm, ObjClass* klass, ObjString* name, uint8_t state) {
    ValueContainer method;
    
    if(!tableGet(&klass -> methods, name, &method)) {
        // Try to find it in statics.
        // I know it can be a value other than a function.
        // But still, this way things are much simpler.

        if(tableGet(&klass -> statics, name, &method)) {
            if(state == 2 || state == 3) {
                RUNTIME_ERROR("Cannot set a static property from an instance!");

                return false;
            }

            pop_state(vm, state);

            PUSH(method.value);

            return true;
        } else {
            RUNTIME_ERROR("Undefined property '%s'!", name -> buffer);
            
            return false;
        }
    }
    
    Value receiver;
    
    switch(state) {
        case 2:
        case 0: receiver = *peek(vm, 0); break;
        
        case 3: 
        case 1: receiver = *peek(vm, 1); break;
    }
    
    ObjBoundMethod* boundMethod = newBoundMethod(vm, receiver, method.value.as.obj);
    
    pop_state(vm, state);
    
    PUSH(OBJECT_VAL(boundMethod));
    
    return true;
}

// Fetches wrapper class.

static ObjClass* fetchClass(Value value) {
    switch(value.type) {
        case VAL_NUMBER: return vmNumberClass;
        case VAL_OBJECT: {
            switch(OBJ_TYPE(value)) {
                case OBJ_LIST: return vmListClass;
                case OBJ_STRING: return vmStringClass;
                case OBJ_DICTIONARY: return vmDictionaryClass;
            }

            break;
        }
    }

    return NULL;
}

static WrapperStatus wrapperProperty(VM* vm, Value value, ObjString* name, uint8_t state) {
    WrapperStatus wrapperStatus;

    wrapperStatus.status   = false;
    wrapperStatus.hadError = false;

    ObjClass* klass = fetchClass(value);

    if(klass == NULL) return wrapperStatus;

    ValueContainer valueContainer;

    if(tableGet(&klass -> methods, name, &valueContainer)) {
        Value receiver;
    
        switch(state) {
            case 2:
            case 0: receiver = *peek(vm, 0); break;
            
            case 3: 
            case 1: receiver = *peek(vm, 1); break;
        }
        
        ObjBoundMethod* boundMethod = newBoundMethod(vm, receiver, valueContainer.value.as.obj);

        value = OBJECT_VAL(boundMethod);
    }
    else if(state == 2 || state == 3) {
        wrapperStatus.hadError = true;

        RUNTIME_ERROR("Attempt to set static property from a wrapper-class value!");

        return wrapperStatus;
    }
    else if(tableGet(&klass -> statics, name, &valueContainer)) 
        value = valueContainer.value;
    else {
        if(IS_DICTIONARY(value)) 
            return wrapperStatus;
        
        wrapperStatus.status   = true;
        wrapperStatus.hadError = true;

        RUNTIME_ERROR("Undefined wrapper property '%s'!", name -> buffer);

        return wrapperStatus;
    }

    pop_state(vm, state);

    PUSH(value);

    wrapperStatus.status = true;

    return wrapperStatus;
}

typedef struct InvokeData {
    bool status;
    char* message;
} InvokeData;

static InvokeData invoke(VM* vm, ObjString* name, uint8_t argCount, uint8_t state, bool isStatic) {
    InvokeData data;

    data.status = false;
    data.message  = NULL;

    if(state == 2u) 
        return data;

    Value value = *peek(vm, argCount);
    
    ValueContainer field;
    
    if(isStatic) {
        if(IS_CLASS(value)) {
            ObjClass* klass = VALUE_CLASS(value);
            
            if(tableGet(&klass -> statics, name, &field)) {
                vm -> stack[vm -> stackTop - argCount - 1u] = field.value;
                
                data.status = callValue(vm, field.value, argCount);

                return data;
            }
            
            data.message = "Undefined static property '%s'!";
            
            return data;
        } else {
            data.message = "Attempt to call a static property from non-class value!";
        
            return data;
        }
    }
    
    Table* fields;

    if(state < 1u) {
        fields = &VALUE_INSTANCE(value) -> fields;

        if(tableGet(fields, name, &field)) {
            vm -> stack[vm -> stackTop - argCount - 1u] = field.value;
            
            data.status = callValue(vm, field.value, argCount);

            return data;
        }

        if(state < 1u && tableGet(&VALUE_INSTANCE(value) -> klass -> methods, name, &field)) {
            data.status = callValue(vm, field.value, argCount);

            return data;
        }
        
        if(state < 1u && tableGet(&VALUE_INSTANCE(value) -> klass -> statics, name, &field)) {
            vm -> stack[vm -> stackTop - argCount - 1u] = field.value;
        
            data.status = callValue(vm, field.value, argCount);

            return data;
        } else {
            data.message = "Undefined property '%s'!";

            return data;
        }
    } else {
        fields = &VALUE_DICTIONARY(value) -> fields;

        if(tableGet(fields, name, &field)) {
            vm -> stack[vm -> stackTop - argCount - 1u] = field.value;
            
            data.status = callValue(vm, field.value, argCount);

            return data;
        } else {
            data.message = "Undefined dictionary property '%s'!";
        
            return data;
        }
    }
    
    // Unreachable.
    
    return data;
}

static WrapperStatus wrapperInvoke(VM* vm, Value value, ObjString* name, uint8_t argCount) {
    WrapperStatus wrapperStatus;

    wrapperStatus.status   = false;
    wrapperStatus.hadError = false;

    ObjClass* klass = fetchClass(value);

    if(klass == NULL) return wrapperStatus;

    ValueContainer method;

    wrapperStatus.status = true;

    if(tableGet(&klass -> methods, name, &method)) {
        wrapperStatus.hadError = !callValue(vm, method.value, argCount);

        return wrapperStatus;
    }

    if(tableGet(&klass -> statics, name, &method)) {
        vm -> stack[vm -> stackTop - argCount - 1u] = method.value;

        wrapperStatus.hadError = !callValue(vm, method.value, argCount);
    } else {
        if(IS_DICTIONARY(value)) {
            wrapperStatus.status = false;

            return wrapperStatus;
        }
        
        wrapperStatus.hadError = true;

        RUNTIME_ERROR("Undefined wrapper property '%s'!", name -> buffer);
    }

    return wrapperStatus;
}

static bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, uint8_t argCount) {
    ValueContainer valueContainer;

    if(tableGet(&klass -> methods, name, &valueContainer)) {
        if(callValue(vm, valueContainer.value, argCount)) 
            return true;
        
        return false;
    } else {
        RUNTIME_ERROR("Undefined property '%s'!", name -> buffer);

        return false;
    }
}

static double incdc(Value* value, bool dec, bool pre) {
    double number = 0;

    switch(value -> type) {
        case VAL_NUMBER: {
            value -> as.number += dec ? -1 : 1;
            
            number = pre ? value -> as.number : value -> as.number + (dec ? 1 : -1);

            break;
        }

        case VAL_BOOLEAN: {
            value -> as.number = !!value -> as.boolean + (dec ? -1 : 1);
            
            number = pre ? value -> as.number : value -> as.number + (dec ? 1 : -1);

            break;
        }

        case VAL_NULL: {
            value -> as.number = dec ? -1 : 1;
            
            number = pre ? value -> as.number : value -> as.number + (dec ? 1 : -1);

            break;
        }

        case VAL_OBJECT: {
            switch(OBJ_TYPE(*value)) {
                case OBJ_STRING: {
                    value -> as.number = pstrtod(VALUE_CSTRING(*value));

                    value -> as.number += dec ? -1 : 1;

                    number = pre ? value -> as.number : value -> as.number + (dec ? 1 : -1);

                    break;
                }

                case OBJ_INSTANCE: {
                    ObjInstance* instance = VALUE_INSTANCE(*value);

                    // Wrapper class.

                    if(instance -> klass == vmNumberClass) {
                        Entry* entry = findEntry(instance -> fields.entries, instance -> fields.capacity, numberField);

                        if(entry != NULL && entry -> key != NULL) 
                            return incdc(&entry -> valueContainer.value, dec, pre);
                    }

                    // No break
                }

                default: value -> as.number = number = NAN;
            }
        }
    }

    value -> type = VAL_NUMBER;

    return number;
}

typedef struct {
    bool hadError;
    bool isRepresentable;
    double number;
} vmNumberData;

static vmNumberData vmToNumber(VM*, Value*);

static vmNumberData vmToNumberRaw(VM* vm, Value* value) {
    vmNumberData data;
    
    data.hadError       = false;
    data.isRepresentable = true;
    data.number        = NAN;
    
    switch(value -> type) {
        case VAL_NUMBER:  data.number = value -> as.number; break;
        case VAL_BOOLEAN: data.number = value -> as.boolean; break;
        case VAL_NULL:    data.number = 0; break;
        
        case VAL_OBJECT: {
            switch(OBJ_TYPE(*value)) {
                case OBJ_DICTIONARY: {
                    Table* fields = &VALUE_DICTIONARY(*value) -> fields;

                    ObjString* represent = TAKE_STRING("__represent__", 13u, false);

                    ValueContainer valueContainer;
                    
                    if(tableGet(fields, represent, &valueContainer)) {
                        Value callable = valueContainer.value;

                        if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
                            stack_push(vm, callable);

                            uint8_t arity = IS_FUNCTION(callable) ? VALUE_FUNCTION(callable) -> arity :
                                         VALUE_CLOSURE(callable) -> function -> arity;
                            
                            if(arity >= 1) {
                                for(uint8_t i = 0u; i < arity; i++) 
                                    stack_push(vm, NULL_VAL);
                            }

                            if(callValue(vm, callable, arity) && run(vm) != INTERPRET_RUNTIME_ERROR) {
                                callable = stack_pop(vm);

                                return vmToNumber(vm, &callable);
                            }
                            
                            data.hadError = true;
                            
                            return data;
                        }
                        else if(IS_NATIVE(callable)) {
                            NativeFn native = VALUE_NATIVE(callable) -> function;

                            NativePack pack = native(vm, 1, (Value*) value);

                            if(!pack.hadError) 
                                return vmToNumber(vm, &pack.value);
                            
                            data.hadError = pack.hadError;
                            
                            return data;
                        }
                    }
                    
                    break;
                }
                
                case OBJ_STRING: {
                    data.number = pstrtod(VALUE_CSTRING(*value));

                    if(!isnan(data.number)) break;
                }

                // And for all other object types, they are not representable 
                // in numbers.

                default: data.isRepresentable = false;
            }

            break;
        }

        default: data.isRepresentable = false;
    }
    
    return data;
}

static vmNumberData vmToNumber(VM* vm, Value* value) {
    vmNumberData data;

    data.number        = NAN;
    data.hadError       = false;
    data.isRepresentable = true;

    if(IS_INSTANCE(*value)) {
        ObjInstance* instance = VALUE_INSTANCE(*value);

        ValueContainer valueContainer;

        ObjString* represent = TAKE_STRING("__represent__", 13u, false);
        ObjString* stringify = TAKE_STRING("stringify", 9u, false);

        bool found = false;

        if((found = tableGet(&instance -> fields, represent, &valueContainer))) {}
        else if((found = tableGet(&instance -> klass -> methods, represent, &valueContainer))) {}
        else if((found = tableGet(&instance -> fields, stringify, &valueContainer))) {}
        else if((found = tableGet(&instance -> klass -> methods, stringify, &valueContainer))) {}

        if(found) {
            Value callable = valueContainer.value;

            if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
                stack_push(vm, callable);

                if(callValue(vm, callable, 0u)) {
                    vm -> stack[vm -> stackTop - 1u] = *value;

                    if(run(vm) != INTERPRET_RUNTIME_ERROR) {
                        callable = stack_pop(vm);

                        return vmToNumberRaw(vm, &callable);
                    }

                    data.hadError = true;

                    return data;
                }

                data.hadError = true;

                return data;
            }
            else if(IS_NATIVE(callable)) {
                NativeFn native = VALUE_NATIVE(callable) -> function;

                NativePack pack = native(vm, 1, (Value*) value);

                if(!pack.hadError) 
                    return vmToNumberRaw(vm, &pack.value);
                
                data.hadError = true;
                
                return data;
            }
        }
    }

    return vmToNumberRaw(vm, value);
}

#define BITWISE(value1, value2, op) \
    NumberData data = toNumber(vm, &value1);\
    if(data.hadError == true) \
        return INTERPRET_RUNTIME_ERROR;\
    double a = data.number;\
    data = toNumber(vm, &value2);\
    if(data.hadError == true) \
        return INTERPRET_RUNTIME_ERROR;\
    double b = data.number;\
    a = (isinf(a) || isnan(a)) ? 0 : a;\
    b = (isinf(b) || isnan(b)) ? 0 : b;\
    if(a > MAX_SAFE_INTEGER || b > MAX_SAFE_INTEGER || a < MIN_SAFE_INTEGER || b < MIN_SAFE_INTEGER) {\
        RUNTIME_ERROR("The provided number is out of bound to be converted to integer!");\
        return INTERPRET_RUNTIME_ERROR;\
    }\
    long long ba = (long long) trunc(a);\
    long long bb = (long long) trunc(b);\
    PUSH(NUMBER_VAL((double) (ba op bb)))\

InterpretResult run(VM* vm) {
    uint8_t currentFrameCount = vm -> frameCount;

    register CallFrame* frame = vm -> frames + (vm -> frameCount - 1u);
    
    register ObjFunction* frame_function = getFunction(frame -> function);
    register ObjClosure* frame_closure = getClosure(frame -> function);
    
    register uint8_t instruction;

    register Value invokeTemp;

    while(true) {
        goto __continue_switch;

        __run_gc: 
            if(vm -> bytesAllocated > vm -> nextGC) 
                garbageCollector();
        
        __continue_switch: 
#if defined(DEBUG) && defined(DEBUG_TRACE_EXECUTION)
        printf("_------------------------------------------------------------------------------------------------------------------------_\n"
                "Frame function : %s\nCurrent stack: [ ", getFunction(frame -> function) -> name -> buffer);

		for(int i = 0; i < vm -> stackTop; i++) {
			printValue(vm -> stack + i);
			
			if(i + 1 != vm -> stackTop) 
				printf(", ");
		}

		printf(" ]\n\nIntruction: ");
		disassembleInstruction(&getFunction(frame -> function) -> chunk, (size_t) (frame -> ip - getFunction(frame -> function) -> chunk.code));
		printf("-________________________________________________________________________________________________________________________-\n\n");
#endif

        switch(instruction = READ_BYTE()) {
            case OP_CONSTANT: PUSH(readConstant(frame, frame_function, false)); break;
            case OP_CONSTANT_LONG: PUSH(readConstant(frame, frame_function, true)); break;

            case OP_DEFINE_GLOBAL: {
                bool isConst = READ_BYTE();
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                if(entry != NULL && entry -> key != NULL) {
                    RUNTIME_ERROR("Redefinition of global variable '%s'!", name -> buffer);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ValueContainer valueContainer;
                
                valueContainer.value   = POP();
                valueContainer.isConst = isConst;
                
                tableInsert(&vm -> globals, name, valueContainer);

                break;
            }

            case OP_DEFINE_GLOBAL_LONG: {
                bool isConst = READ_BYTE();
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                if(entry != NULL && entry -> key != NULL) {
                    RUNTIME_ERROR("Redefinition of global variable '%s'!", name -> buffer);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ValueContainer valueContainer;
                
                valueContainer.value   = POP();
                valueContainer.isConst = isConst;
                
                tableInsert(&vm -> globals, name, valueContainer);

                break;
            }

            case OP_GET_GLOBAL: {
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));

                ValueContainer valueContainer;

                if(!tableGet(&vm -> globals, name, &valueContainer)) {
                    RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);
                    return INTERPRET_RUNTIME_ERROR;
                }

                PUSH(valueContainer.value);

                break;
            }

            case OP_GET_GLOBAL_LONG: {
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));

                ValueContainer valueContainer;

                if(!tableGet(&vm -> globals, name, &valueContainer)) {
                    RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);
                    return INTERPRET_RUNTIME_ERROR;
                }

                PUSH(valueContainer.value);

                break;
            }

            case OP_SET_GLOBAL: {
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                if(entry != NULL && entry -> key != NULL) {
                    if(entry -> valueContainer.isConst) {
                        RUNTIME_ERROR("Attempt to reassign constant global variable '%s'!", name -> buffer);
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    ValueContainer valueContainer;
                
                    valueContainer.isConst = false;
                    valueContainer.value   = *peek(vm, 0);
                    
                    entry -> valueContainer = valueContainer;
                } else {
                    RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }

                goto __run_gc;
            }
            
            case OP_SET_GLOBAL_LONG: {
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                if(entry != NULL && entry -> key != NULL) {
                    if(entry -> valueContainer.isConst) {
                        RUNTIME_ERROR("Attempt to reassign constant global variable '%s'!", name -> buffer);
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    ValueContainer valueContainer;
                
                    valueContainer.isConst = false;
                    valueContainer.value   = *peek(vm, 0);
                    
                    entry -> valueContainer = valueContainer;
                } else {
                    RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }

                goto __run_gc;
            }
            
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
            case OP_GET_LOCAL_20: {
                PUSH(vm -> stack[frame -> slots + (instruction - OP_GET_LOCAL_0)]);
                
                break;
            }
            
            case OP_GET_LOCAL: {
                uint32_t slot = readBytes(frame, false);
                
                PUSH(vm -> stack[frame -> slots + slot]);
                
                break;
            }
            
            case OP_GET_LOCAL_LONG: {
                uint32_t slot = readBytes(frame, true);
                
                PUSH(vm -> stack[frame -> slots + slot]);
                
                break;
            }
            
            case OP_SET_LOCAL: {
                uint32_t slot = readBytes(frame, false);
                
                vm -> stack[frame -> slots + slot] = *peek(vm, 0);
                
                goto __run_gc;
            }
            
            case OP_SET_LOCAL_LONG: {
                uint32_t slot = readBytes(frame, true);
                
                vm -> stack[frame -> slots + slot] = *peek(vm, 0);
                
                goto __run_gc;
            }
            
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
            case OP_SET_LOCAL_20: {
                vm -> stack[frame -> slots + (instruction - OP_SET_LOCAL_0)] = *peek(vm, 0);
                
                goto __run_gc;
            }
            
            case OP_GET_UPVALUE: {
                uint8_t slot = readBytes(frame, false);
                
                PUSH(*frame_closure -> upvalues[slot] -> location);
                
                break;
            }
            
            case OP_GET_UPVALUE_LONG: {
                uint32_t slot = readBytes(frame, true);
                
                PUSH(*frame_closure -> upvalues[slot] -> location);
                
                break;
            }
            
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
            case OP_GET_UPVALUE_20: {
                PUSH(*frame_closure -> upvalues[instruction - OP_GET_UPVALUE_0] -> location);
                
                break;
            }
            
            case OP_SET_UPVALUE: {
                uint8_t slot = readBytes(frame, false);
                
                if(frame_closure -> upvalues[slot] -> isConst) {
                    RUNTIME_ERROR("Attempt to reassign a constant upvalue!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                *frame_closure -> upvalues[slot] -> location = *peek(vm, 0);
                
                goto __run_gc;
            }
            
            case OP_SET_UPVALUE_LONG: {
                uint8_t slot = readBytes(frame, true);
                
                if(frame_closure -> upvalues[slot] -> isConst) {
                    RUNTIME_ERROR("Attempt to reassign a constant upvalue!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                *frame_closure -> upvalues[slot] -> location = *peek(vm, 0);
                
                goto __run_gc;
            }
            
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
            case OP_SET_UPVALUE_20: {
                uint8_t slot = instruction - OP_SET_UPVALUE_0;
                
                if(frame_closure -> upvalues[slot] -> isConst) {
                    RUNTIME_ERROR("Attempt to reassign a constant upvalue!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                *frame_closure -> upvalues[slot] -> location = *peek(vm, 0);
                
                goto __run_gc;
            }
            
            case OP_CALL: {
                uint8_t argCount = READ_BYTE();
                
                if(!callValue(vm, *peek(vm, argCount), argCount)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = vm -> frames + (vm -> frameCount - 1u);
                
                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);
                
                break;
            }
            
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
            case OP_CALL_20: {
                uint8_t argCount = instruction - OP_CALL_0;
                
                if(!callValue(vm, *peek(vm, argCount), argCount)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = vm -> frames + (vm -> frameCount - 1u);
                
                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);
                
                break;
            }
            
            case OP_NEGATE: {
                Value value = POP();
                
                NumberData data = toNumber(vm, &value);

                if(data.hadError) 
                    return INTERPRET_RUNTIME_ERROR;
                
                PUSH(NUMBER_VAL(-data.number));

                break;
            }

            case OP_ADD: {
#define CONCATENATE(value1, value2) concat(vm, (value1), (value2))

#define NRM_ADD(value) \
    char* s = (value);\
    char* str = toString(vm, &value2);\
    char* result = CONCATENATE(s, str);\
    free(str);\
    PUSH(OBJECT_VAL(TAKE_STRING(result, strlen(result), true)));
                Value value2 = POP(),
                     value1 = POP();

                if(IS_NUMBER(value1)) {
                    double a = VALUE_NUMBER(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL(a + b));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a + b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(a));
                    else if(IS_OBJECT(value2)) {
                        vmNumberData data = vmToNumber(vm, &value2);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        if(data.isRepresentable) {
                            PUSH(NUMBER_VAL(a + data.number));
                        } else {
                            char* str1 = toString(vm, &value1);
                            char* str2 = toString(vm, &value2);

                            char* result = CONCATENATE(str1, str2);

                            PUSH(OBJECT_VAL(TAKE_STRING(result, strlen(result), true)));

                            free(str1);
                            free(str2);
                        }
                    }
                }
                else if(IS_BOOL(value1)) {
                    bool a = VALUE_BOOL(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL(b + a));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a + b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL((double) a));
                    else if(IS_OBJECT(value2)) {
                        vmNumberData data = vmToNumber(vm, &value2);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        if(data.isRepresentable) {
                            PUSH(NUMBER_VAL(a + data.number));
                        } else { NRM_ADD(a ? "true" : "false"); }
                    }
                }
                else if(IS_NULL(value1)) {
                    if(IS_NUMBER(value2)) PUSH(value2);
                    else if(IS_BOOL(value2)) {
                        value2.type = VAL_NUMBER;
                        value2.as.number = value2.as.boolean;

                        PUSH(value2);
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(0));
                    else if(IS_OBJECT(value2)) {
                        vmNumberData data = vmToNumber(vm, &value2);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        if(data.isRepresentable) {
                            PUSH(NUMBER_VAL(data.number));
                        } else { NRM_ADD("null"); }
                    }
                }
                else if(IS_STRING(value1)) {
                    char* result = toString(vm, &value2);
                    char* string = CONCATENATE(VALUE_CSTRING(value1), result);

                    PUSH(OBJECT_VAL(TAKE_STRING(string, strlen(string), true)));

                    free(result);
                }
                else if(IS_OBJECT(value1)) {
                    vmNumberData data = vmToNumber(vm, &value1);
                        
                    if(data.hadError) 
                        return INTERPRET_RUNTIME_ERROR;
                    
                    if(data.isRepresentable) {
                        double a = data.number;
                        
                        data = vmToNumber(vm, &value2);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        PUSH(NUMBER_VAL(a + data.number));
                    } else {
                        char* result1 = toString(vm, &value1);
                        char* result2 = toString(vm, &value2);

                        char* string = CONCATENATE(result1, result2);
                        
                        PUSH(OBJECT_VAL(TAKE_STRING(string, strlen(string), true)));

                        free(result1);
                        free(result2);
                    }
                }

                break;
#undef NRM_ADD
#undef CONCATENATE
            }

#define PUSH_NAN PUSH(NUMBER_VAL(NAN))

            case OP_SUBSTRACT: {
#define NRM_SUB(a) \
    PUSH(NUMBER_VAL(a - pstrtod(VALUE_CSTRING(value2))))
                Value value2 = POP(),
                     value1 = POP();

                if(IS_NUMBER(value1)) {
                    double a = VALUE_NUMBER(value1);

                    if(IS_NUMBER(value2)) {
                        if(!IS_NAN(value2)) {
                            double b = VALUE_NUMBER(value2);

                            PUSH(NUMBER_VAL(a - b));
                        } else PUSH_NAN;
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a - b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(a));
                    else if(IS_STRING(value2)) { NRM_SUB(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a - data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_BOOL(value1)) {
                    bool a = VALUE_BOOL(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL((double) (a) - b));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a - b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL((double) a));
                    else if(IS_STRING(value2)) { NRM_SUB((double) a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a - data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_NULL(value1)) {
                    if(IS_NUMBER(value2)) {
                        value2.as.number = -value2.as.number;

                        PUSH(value2);
                    }
                    else if(IS_BOOL(value2)) {
                        value2.type = VAL_NUMBER;
                        value2.as.number = -value2.as.boolean;

                        PUSH(value2);
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(0));
                    else if(IS_STRING(value2)) { NRM_SUB(0); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(-data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_STRING(value1)) {
#define nrm(value) PUSH(NUMBER_VAL(a - (value)))
                    double a = pstrtod(VALUE_CSTRING(value1));

                    if(IS_NUMBER(value2)) nrm(VALUE_NUMBER(value2));
                    else if(IS_BOOL(value2)) nrm(VALUE_BOOL(value2));
                    else if(IS_NULL(value2)) nrm(0);
                    else if(IS_STRING(value2)) nrm(pstrtod(VALUE_CSTRING(value2)));
                    else PUSH_NAN;
#undef nrm
                } else {
                    vmNumberData data = vmToNumber(vm, &value1);

                    if(data.hadError) 
                        return INTERPRET_RUNTIME_ERROR;

                    if(data.isRepresentable) {
                        double a = data.number;

                        data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        PUSH(NUMBER_VAL(a - data.number));
                    } else PUSH_NAN;
                }

                break;
#undef NRM_SUB
            }
            
            case OP_MULTIPLY: {
#define NRM_MUL(a) \
    PUSH(NUMBER_VAL((a) * pstrtod(VALUE_CSTRING(value2))))
                Value value2 = POP(),
                     value1 = POP();

                if(IS_NUMBER(value1)) {
                    double a = VALUE_NUMBER(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL(a * b));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a * b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(a * 0));
                    else if(IS_STRING(value2)) { NRM_MUL(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a * data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_BOOL(value1)) {
                    bool a = VALUE_BOOL(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL((double) b * a));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a * b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(0));
                    else if(IS_STRING(value2)) { NRM_MUL(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a * data.number));
                        else PUSH_NAN;
                    };
                }
                else if(IS_NULL(value1)) {
                    if(IS_NUMBER(value2)) 
                        PUSH(NUMBER_VAL(0 * VALUE_NUMBER(value2)));
                    else if(IS_STRING(value2)) 
                        PUSH(NUMBER_VAL(0 * pstrtod(VALUE_CSTRING(value2))));
                    else if(IS_OBJECT(value2)) {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(0 * data.number));
                        else PUSH_NAN;
                    } else PUSH(NUMBER_VAL(0));
                }
                else if(IS_STRING(value1)) {
#define nrm(value) PUSH(NUMBER_VAL(a * (value)))
                    double a = pstrtod(VALUE_CSTRING(value1));

                    if(IS_NUMBER(value2)) nrm(VALUE_NUMBER(value2));
                    else if(IS_BOOL(value2)) nrm(VALUE_BOOL(value2));
                    else if(IS_NULL(value2)) nrm(0);
                    else if(IS_STRING(value2)) nrm(pstrtod(VALUE_CSTRING(value2)));
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a * data.number));
                        else PUSH_NAN;
                    }
#undef nrm
                } else {
                    vmNumberData data = vmToNumber(vm, &value1);

                    if(data.hadError) 
                        return INTERPRET_RUNTIME_ERROR;

                    if(data.isRepresentable) {
                        double a = data.number;

                        data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        PUSH(NUMBER_VAL(a * data.number));
                    } else PUSH_NAN;
                }

                break;
#undef NRM_MUL
            }
            
            case OP_DIVIDE: {
#define NRM_DIV(a) \
    PUSH(NUMBER_VAL((a) / pstrtod(VALUE_CSTRING(value2))))
                Value value2 = POP(),
                     value1 = POP();

                if(IS_NUMBER(value1)) {
                    double a = VALUE_NUMBER(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL(a / b));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a / b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(isnan(a) ? NAN : INFINITY));
                    else if(IS_STRING(value2)) { NRM_DIV(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a / data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_BOOL(value1)) {
                    bool a = VALUE_BOOL(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL((double) a / b));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(a / b));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH(NUMBER_VAL(!a ? NAN : INFINITY));
                    else if(IS_STRING(value2)) { NRM_DIV(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a / data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_NULL(value1)) {
                    if(IS_NUMBER(value2)) PUSH(NUMBER_VAL(0 / VALUE_NUMBER(value2)));
                    else if(IS_STRING(value2)) PUSH(NUMBER_VAL(0 / pstrtod(VALUE_CSTRING(value2))));
                    else if(IS_BOOL(value2)) PUSH(NUMBER_VAL(0 / VALUE_BOOL(value2)));
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(0 / data.number));
                        else PUSH_NAN;
                    }
                }
                else if(IS_STRING(value1)) {
#define nrm(value) PUSH(NUMBER_VAL(a / (value)))
                    double a = pstrtod(VALUE_CSTRING(value1));

                    if(IS_NUMBER(value2)) nrm(VALUE_NUMBER(value2));
                    else if(IS_BOOL(value2)) nrm(VALUE_BOOL(value2));
                    else if(IS_NULL(value2)) PUSH(NUMBER_VAL(isnan(a) ? NAN : INFINITY));
                    else if(IS_STRING(value2)) nrm(pstrtod(VALUE_CSTRING(value2)));
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(a / data.number));
                        else PUSH_NAN;
                    }
#undef nrm
                } else {
                    vmNumberData data = vmToNumber(vm, &value1);

                    if(data.hadError) 
                        return INTERPRET_RUNTIME_ERROR;

                    if(data.isRepresentable) {
                        double a = data.number;

                        data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        PUSH(NUMBER_VAL(a / data.number));
                    } else PUSH_NAN;
                }

                break;
#undef NRM_DIV
            }
            
            case OP_MODULUS: {
#define NRM_MOD(value) \
    PUSH(NUMBER_VAL(fmod((value), pstrtod(VALUE_CSTRING(value2)))))
                Value value2 = POP(),
                     value1 = POP();

                if(IS_NUMBER(value1)) {
                    double a = VALUE_NUMBER(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL(fmod(a, b)));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(fmod(a, b)));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH_NAN;
                    else if(IS_STRING(value2)) { NRM_MOD(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(fmod(a, data.number)));
                        else PUSH_NAN;
                    }
                }
                else if(IS_BOOL(value1)) {
                    bool a = VALUE_BOOL(value1);

                    if(IS_NUMBER(value2)) {
                        double b = VALUE_NUMBER(value2);

                        PUSH(NUMBER_VAL(fmod(b, a)));
                    }
                    else if(IS_BOOL(value2)) {
                        bool b = VALUE_BOOL(value2);

                        PUSH(NUMBER_VAL(fmod(a, b)));
                    }
                    else if(IS_NULL(value2)) 
                        PUSH_NAN;
                    else if(IS_STRING(value2)) { NRM_MOD(a); }
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(fmod(a, data.number)));
                        else PUSH_NAN;
                    }
                }
                else if(IS_NULL(value1)) {
                    if(IS_NUMBER(value2)) 
                        PUSH(NUMBER_VAL(fmod(0, VALUE_NUMBER(value1))));
                    else if(IS_STRING(value2)) 
                        PUSH(NUMBER_VAL(fmod(0, pstrtod(VALUE_CSTRING(value2)))));
                    else if(IS_BOOL(value2)) 
                        PUSH(NUMBER_VAL(fmod(0, VALUE_BOOL(value2))));
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(fmod(0, data.number)));
                        else PUSH_NAN;
                    }
                }
                else if(IS_STRING(value1)) {
#define nrm(value) PUSH(NUMBER_VAL(fmod(a, value)))
                    double a = pstrtod(VALUE_CSTRING(value1));

                    if(IS_NUMBER(value2)) nrm(VALUE_NUMBER(value2));
                    else if(IS_BOOL(value2)) nrm(VALUE_BOOL(value2));
                    else if(IS_NULL(value2)) PUSH_NAN;
                    else if(IS_STRING(value2)) nrm(pstrtod(VALUE_CSTRING(value2)));
                    else {
                        vmNumberData data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        if(data.isRepresentable) 
                            PUSH(NUMBER_VAL(fmod(a, data.number)));
                        else PUSH_NAN;
                    }
#undef nrm
                } else {
                    vmNumberData data = vmToNumber(vm, &value1);

                    if(data.hadError) 
                        return INTERPRET_RUNTIME_ERROR;

                    if(data.isRepresentable) {
                        double a = data.number;

                        data = vmToNumber(vm, &value2);

                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;

                        PUSH(NUMBER_VAL(fmod(a, data.number)));
                    } else PUSH_NAN;
                }

                break;
#undef NRM_DIV
            }

            case OP_TRUE:    PUSH(BOOL_VAL(true)); break;
            case OP_FALSE:    PUSH(BOOL_VAL(false)); break;
            case OP_NULL:    PUSH(NULL_VAL); break;
            case OP_INFINITY: PUSH(NUMBER_VAL(INFINITY)); break;
            case OP_NAN:     PUSH_NAN; break;
        
#undef PUSH_NAN
            
            case OP_EQUAL:    PUSH(BOOL_VAL(valuesEqual(POP(), POP()))); break;
            case OP_NOT_EQUAL: PUSH(BOOL_VAL(!valuesEqual(POP(), POP()))); break;
            
            case OP_GREATER:      VALUE_COMP(>);
            case OP_LESS:        VALUE_COMP(<);
            case OP_GREATER_EQUAL: VALUE_COMP(>=);
            case OP_LESS_EQUAL:    VALUE_COMP(<=);
            
            case OP_JUMP_IF_FALSE: {
                // Used by the conditional statements and loops. Could use OP_JUMP_OPR,
                // but it will likely to give a slight performance cost.
                
                uint32_t stride = readBytes(frame, true);
                
                Value value = POP();

                BooleanData data = toBoolean(vm, &value);

                if(data.hadError) 
                    return INTERPRET_RUNTIME_ERROR;
                
                if(!data.boolean) 
                    frame -> ip += stride;
                
                break;
            }
            
            case OP_JUMP_OPR: {
                // Used by the operators. Keeps the value on the stack to be used.
                
                uint32_t stride = readBytes(frame, true);

                BooleanData data = toBoolean(vm, peek(vm, 0));

                if(data.hadError) 
                    return INTERPRET_RUNTIME_ERROR;
                
                if(!data.boolean) 
                    frame -> ip += stride;
                
                break;
            }
            
            case OP_JUMP: frame -> ip += readBytes(frame, true); break;
            
            case OP_LOOP: {
                uint32_t param = readBytes(frame, true);
                
                frame -> ip -= param;
                
                break;
            }

            case OP_NOT: {
                Value* value = peek(vm, 0);

                BooleanData data = toBoolean(vm, value);

                if(data.hadError) 
                    return INTERPRET_RUNTIME_ERROR;
                
                value -> as.boolean = !data.boolean;
                value -> type = VAL_BOOLEAN;
                
                break;
            }
            
            case OP_BITWISE_NEGATION: {
                Value value = POP();

                NumberData data = toNumber(vm, &value);

                if(data.hadError) 
                    return INTERPRET_RUNTIME_ERROR;
                
                if(data.number > MAX_SAFE_INTEGER || data.number < MIN_SAFE_INTEGER) {
                    RUNTIME_ERROR("The provided number is out of bound to be converted to integer!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                long long inum = (long long) data.number;

                PUSH(NUMBER_VAL((double) ~inum));
                
                break;
            }

            case OP_INSTOF: {
                if(!IS_CLASS(*peek(vm, 0))) {
                    RUNTIME_ERROR("Expected the Right Hand Side of 'instof' to be a class!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* klass = VALUE_CLASS(POP());

                Value instance  = POP();

                ObjClass* check = NULL;


                if(IS_INSTANCE(instance)) 
                    check = VALUE_INSTANCE(instance) -> klass;
                else {
                    // Wrapper class.

                    switch(instance.type) {
                        case VAL_NUMBER: check = vmNumberClass; break;
                        case VAL_OBJECT: {
                            switch(OBJ_TYPE(instance)) {
                                case OBJ_LIST: check = vmListClass; break;
                                case OBJ_STRING: check = vmStringClass; break;
                                case OBJ_DICTIONARY: check = vmDictionaryClass; break;
                                case OBJ_FUNCTION:
                                case OBJ_CLOSURE:
                                case OBJ_BOUND_METHOD:
                                case OBJ_NATIVE:
                                    check = vmFunctionClass;
                                
                                case OBJ_UPVALUE: 
                                case OBJ_CLASS: 
                                case OBJ_INSTANCE: 
                                case OBJ_FILE: 
                                case OBJ_BYTELIST: break;
                            }

                            break;
                        }

                        default: ;
                    }
                }

                bool result = false;

                while(check != NULL && !(result = check == klass)) 
                    check = check -> superClass;
                
                PUSH(BOOL_VAL(result));

                break;
            }

            case OP_SHOW: {
                Value value = POP();

                if(!printValue(&value))
                    return INTERPRET_RUNTIME_ERROR;

                if(vm -> inREPL) __printf("\n");

                break;
            }
            
            case OP_RAW_SHOW: {
                Value value = POP();
                
                if(!printValue(&value))
                    return INTERPRET_RUNTIME_ERROR;
                
                break;
            }

            case OP_SHOWL: {
                Value value = POP();

                if(!printValue(&value))
                    return INTERPRET_RUNTIME_ERROR;
                __printf("\n");

                break;
            }
            
            case OP_TYPEOF: {
                Value value = POP();
                
                switch(value.type) {
                    case VAL_BOOLEAN: 
                        value = OBJECT_VAL(TAKE_STRING("boolean", 7u, false));
                        break;
                    case VAL_NULL: 
                        value = OBJECT_VAL(TAKE_STRING("null", 4u, false));
                        break;
                    case VAL_NUMBER: 
                        value = OBJECT_VAL(TAKE_STRING("number", 6u, false));
                        break;
                    case VAL_OBJECT: {
                        switch(OBJ_TYPE(value)) {
                            case OBJ_STRING: 
                                value = OBJECT_VAL(TAKE_STRING("string", 6u, false));
                                break;
                            
                            case OBJ_CLOSURE: 
                                value = OBJECT_VAL(TAKE_STRING("closure", 7u, false));
                                break;

                            case OBJ_FUNCTION: 
                                value = OBJECT_VAL(TAKE_STRING("function", 8u, false));
                                break;

                            case OBJ_NATIVE: 
                                value = OBJECT_VAL(TAKE_STRING("native", 6u, false));
                                break;

                            case OBJ_CLASS: 
                                value = OBJECT_VAL(TAKE_STRING("class", 5u, false));
                                break;
                            
                            case OBJ_INSTANCE: 
                                value = OBJECT_VAL(TAKE_STRING("instance", 8u, false));
                                break;
                            
                            case OBJ_BOUND_METHOD: 
                                value = OBJECT_VAL(TAKE_STRING("method", 6u, false));
                                break;
                            
                            case OBJ_DICTIONARY: 
                                value = OBJECT_VAL(TAKE_STRING("dictionary", 10u, false));
                                break;
                            
                            case OBJ_LIST: 
                                value = OBJECT_VAL(TAKE_STRING("list", 4u, false));
                                break;
                            
                            case OBJ_FILE: 
                                value = OBJECT_VAL(TAKE_STRING("file", 4u, false));
                                break;
                            
                            case OBJ_BYTELIST: 
                                value = OBJECT_VAL(TAKE_STRING("bytelist", 8u, false));
                                break;
                            
                            case OBJ_UPVALUE: abort();
                        }
                        
                        break;
                    }
                    
                    default: value = OBJECT_VAL(TAKE_STRING("unknown", 7u, false));
                }
                
                value.type = VAL_OBJECT;
                
                PUSH(value);
                
                break;
            }

            case OP_POP: {
                Value value = POP();

                if(vm -> inREPL) {
                    switch(value.type) {
                        case VAL_NUMBER: 
                            __printf("(Number) ");
                            break;
                        
                        case VAL_BOOLEAN: 
                            __printf("(Boolean) ");
                            break;
                        
                        case VAL_NULL: 
                            __printf("(Empty) ");
                            break;
                        
                        case VAL_OBJECT: {
                            switch(OBJ_TYPE(value)) {
                                case OBJ_STRING: 
                                    __printf("(String) ");
                                    break;
                                
                                case OBJ_CLOSURE: 
                                    __printf("(Closure) ");
                                    break;

                                case OBJ_FUNCTION: 
                                    __printf("(Function) ");
                                    break;
                                
                                case OBJ_NATIVE: 
                                    __printf("(Native) ");
                                    break;

                                case OBJ_CLASS: 
                                    __printf("(Class) ");
                                    break;
                                
                                case OBJ_INSTANCE: 
                                    __printf("(Instance) ");
                                    break;
                                
                                case OBJ_BOUND_METHOD: 
                                    __printf("(Method) ");
                                    break;
                                
                                case OBJ_DICTIONARY: 
                                    __printf("(Dictionary) ");
                                    break;
                                
                                case OBJ_LIST: 
                                    __printf("(List) ");
                                    break;
                                
                                case OBJ_FILE: 
                                    __printf("(File) ");
                                    break;
                                
                                case OBJ_BYTELIST: 
                                    __printf("(ByteList) ");
                                    break;
                                
                                case OBJ_UPVALUE: break;
                            }
                            
                            break;
                        }
                    }
                    
                    if(IS_STRING(value) || (IS_INSTANCE(value) && !strcmp(VALUE_INSTANCE(value) -> klass -> name -> buffer, "String"))) {
                        __printf("'");
                        if(!printValue(&value))
                            return INTERPRET_RUNTIME_ERROR;
                        __printf("'");
                    } else if(!printValue(&value))
                        return INTERPRET_RUNTIME_ERROR;

                    __printf("\n");
                }

                break;
            }
            
            case OP_POST_INCREMENT: {
                uint8_t param = READ_BYTE();
                
                if(param <= 1) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 0 ? false : true));

                    Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant global variable '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, false)));
                    } else {
                        RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if(param > 1 && param <= 3) {
                    // Local.
                    
                    uint32_t slot = readBytes(frame, param == 2 ? false : true);
                
                    PUSH(NUMBER_VAL(incdc(vm -> stack + frame -> slots + slot, false, false)));
                }
                else if(param > 3 && param <= 5) {
                    // Upvalue.

                    uint8_t slot = readBytes(frame, param == 4 ? false : true);

                    ObjUpvalue* upvalue = frame_closure -> upvalues[slot];
                
                    if(upvalue -> isConst) {
                        RUNTIME_ERROR("Attempt to increment a constant upvalue!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    PUSH(NUMBER_VAL(incdc(upvalue -> location, false, false)));
                }
                else if(param > 5 && param <= 7) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 6u ? false : true));

                    Value value = *peek(vm, 0);
                
                    Table* fields;
                    
                    bool isDictionary = false;

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, false)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to increment an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to increment a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, false)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to increment a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                else if(param == 8) {
                    Value value = *peek(vm, 1);
                
                    Value string = *peek(vm, 0);
                    
                    if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) || 
                    IS_BYTELIST(value)) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) {
                            RUNTIME_ERROR("Index is not convertable to number!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        int index = (int) idxval;
                        
                        if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass)) {
                            ObjList* list = NULL;

                            if(IS_LIST(value)) list = VALUE_LIST(value);
                            else {
                                ObjInstance* instance = VALUE_INSTANCE(value);

                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, listField, &valueContainer);

                                list = VALUE_LIST(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += list -> count;
                            
                            if(index + 1 > list -> count || index < 0) {
                                RUNTIME_ERROR("List index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Property
                            POP();    // String.
                            
                            PUSH(NUMBER_VAL(incdc(list -> values + index, false, false)));
                            
                            break;
                        }
                        else if(IS_BYTELIST(value)) {
                            ObjByteList* byteList = VALUE_BYTELIST(value);
                            
                            if(index < 0) 
                                index += byteList -> size;
                            
                            if(index + 1u > byteList -> size || index < 0) {
                                RUNTIME_ERROR("ByteList index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();
                            POP();
                            
                            PUSH(NUMBER_VAL((double) byteList -> bytes[index]));
                            
                            if(byteList -> bytes[index] + 1u <= 255) 
                                byteList -> bytes[index]++;
                            
                            break;
                        }
                    }
                    
                    Table* fields;
                    
                    bool isDictionary = false;

                    char* result = toString(vm, (Value* const) &string);
                    size_t size  = strlen(result);

                    vm -> bytesAllocated += size + 1u;

                    ObjString* name = TAKE_STRING(result, size, true);

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, false)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to increment an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to increment a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, false)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to increment a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    else {
                        RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    Value value = *peek(vm, 0);
                
                    if(!IS_CLASS(value)) {
                        RUNTIME_ERROR("Attempt to access static property from non-class! If it's an instance try using '.'!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    ObjClass* klass = VALUE_CLASS(value);
                    
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 9 ? false : true));
                    
                    Entry* entry = klass -> statics.count ? findEntry(klass -> statics.entries, klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant static property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, false)));
                    } else {
                        RUNTIME_ERROR("Attempt to increment an undefined static property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                break;
            }
            
            case OP_POST_DECREMENT: {
                uint8_t param = READ_BYTE();
                
                if(param <= 1) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 0 ? false : true));

                    Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant global variable '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, false)));
                    } else {
                        RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if(param > 1 && param <= 3) {
                    // Local.
                    
                    uint32_t slot = readBytes(frame, param == 2 ? false : true);
                
                    PUSH(NUMBER_VAL(incdc(vm -> stack + frame -> slots + slot, true, false)));
                }
                else if(param > 3 && param <= 5) {
                    // Upvalue.

                    uint8_t slot = readBytes(frame, param == 4 ? false : true);

                    ObjUpvalue* upvalue = frame_closure -> upvalues[slot];
                
                    if(upvalue -> isConst) {
                        RUNTIME_ERROR("Attempt to decrement a constant upvalue!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    PUSH(NUMBER_VAL(incdc(upvalue -> location, true, false)));
                }
                else if(param > 5 && param <= 7) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 6u ? false : true));

                    Value value = *peek(vm, 0);
                
                    Table* fields;
                    
                    bool isDictionary = false;

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, false)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to decrement an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to decrement a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, false)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to decrement a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                else if(param == 8) {
                    Value value = *peek(vm, 1);
                
                    Value string = *peek(vm, 0);
                    
                    if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) || 
                    IS_BYTELIST(value)) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) {
                            RUNTIME_ERROR("Index is not convertable to number!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        int index = (int) idxval;
                        
                        if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass)) {
                            ObjList* list = NULL;

                            if(IS_LIST(value)) list = VALUE_LIST(value);
                            else {
                                ObjInstance* instance = VALUE_INSTANCE(value);

                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, listField, &valueContainer);

                                list = VALUE_LIST(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += list -> count;
                            
                            if(index + 1 > list -> count || index < 0) {
                                RUNTIME_ERROR("List index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Property
                            POP();    // String.
                            
                            PUSH(NUMBER_VAL(incdc(list -> values + index, true, false)));
                            
                            break;
                        }
                        else if(IS_BYTELIST(value)) {
                            ObjByteList* byteList = VALUE_BYTELIST(value);
                            
                            if(index < 0) 
                                index += byteList -> size;
                            
                            if(index + 1u > byteList -> size || index < 0) {
                                RUNTIME_ERROR("ByteList index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();
                            POP();
                            
                            PUSH(NUMBER_VAL((double) byteList -> bytes[index]));
                            
                            if(byteList -> bytes[index] - 1 >= 0) 
                                byteList -> bytes[index]--;
                            
                            break;
                        }
                    }
                    
                    Table* fields;
                    
                    bool isDictionary = false;

                    char* result = toString(vm, (Value* const) &string);
                    size_t size  = strlen(result);

                    vm -> bytesAllocated += size + 1u;

                    ObjString* name = TAKE_STRING(result, size, true);

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, false)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to decrement an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to decrement a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, false)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to decrement a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    else {
                        RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    Value value = *peek(vm, 0);
                
                    if(!IS_CLASS(value)) {
                        RUNTIME_ERROR("Attempt to access static property from non-class! If it's an instance try using '.'!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    ObjClass* klass = VALUE_CLASS(value);
                    
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 9 ? false : true));
                    
                    Entry* entry = klass -> statics.count ? findEntry(klass -> statics.entries, klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant static property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, false)));
                    } else {
                        RUNTIME_ERROR("Attempt to decrement an undefined static property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                break;
            }
            
            case OP_PRE_INCREMENT: {
                uint8_t param = READ_BYTE();
                
                if(param <= 1) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 0 ? false : true));

                    Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant global variable '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, true)));
                    } else {
                        RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if(param > 1 && param <= 3) {
                    // Local.
                    
                    uint32_t slot = readBytes(frame, param == 2 ? false : true);
                
                    PUSH(NUMBER_VAL(incdc(vm -> stack + frame -> slots + slot, false, true)));
                }
                else if(param > 3 && param <= 5) {
                    // Upvalue.

                    uint8_t slot = readBytes(frame, param == 4 ? false : true);

                    ObjUpvalue* upvalue = frame_closure -> upvalues[slot];
                
                    if(upvalue -> isConst) {
                        RUNTIME_ERROR("Attempt to increment a constant upvalue!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    PUSH(NUMBER_VAL(incdc(upvalue -> location, false, true)));
                }
                else if(param > 5 && param <= 7) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 6u ? false : true));

                    Value value = *peek(vm, 0);
                
                    Table* fields;
                    
                    bool isDictionary = false;

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, true)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to increment an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to increment a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, true)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to increment a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                else if(param == 8) {
                    Value value = *peek(vm, 1);
                
                    Value string = *peek(vm, 0);
                    
                    if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) || 
                    IS_BYTELIST(value)) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) {
                            RUNTIME_ERROR("Index is not convertable to number!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        int index = (int) idxval;
                        
                        if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass)) {
                            ObjList* list = NULL;

                            if(IS_LIST(value)) list = VALUE_LIST(value);
                            else {
                                ObjInstance* instance = VALUE_INSTANCE(value);

                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, listField, &valueContainer);

                                list = VALUE_LIST(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += list -> count;
                            
                            if(index + 1 > list -> count || index < 0) {
                                RUNTIME_ERROR("List index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Property
                            POP();    // String.
                            
                            PUSH(NUMBER_VAL(incdc(list -> values + index, false, true)));
                            
                            break;
                        }
                        else if(IS_BYTELIST(value)) {
                            ObjByteList* byteList = VALUE_BYTELIST(value);
                            
                            if(index < 0) 
                                index += byteList -> size;
                            
                            if(index + 1u > byteList -> size || index < 0) {
                                RUNTIME_ERROR("ByteList index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();
                            POP();
                            
                            if(byteList -> bytes[index] + 1u <= 255) 
                                byteList -> bytes[index]++;
                            
                            PUSH(NUMBER_VAL((double) byteList -> bytes[index]));
                            
                            break;
                        }
                    }
                    
                    Table* fields;
                    
                    bool isDictionary = false;

                    char* result = toString(vm, (Value* const) &string);
                    size_t size  = strlen(result);

                    vm -> bytesAllocated += size + 1u;

                    ObjString* name = TAKE_STRING(result, size, true);

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, true)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to increment an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to increment a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, true)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to increment a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    else {
                        RUNTIME_ERROR("Attempt to increment an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    Value value = *peek(vm, 0);
                
                    if(!IS_CLASS(value)) {
                        RUNTIME_ERROR("Attempt to access static property from non-class! If it's an instance try using '.'!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    ObjClass* klass = VALUE_CLASS(value);
                    
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 9 ? false : true));
                    
                    Entry* entry = klass -> statics.count ? findEntry(klass -> statics.entries, klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to increment a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, false, true)));
                    } else {
                        RUNTIME_ERROR("Attempt to increment an undefined static property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                break;
            }
            
            case OP_PRE_DECREMENT: {
                uint8_t param = READ_BYTE();
                
                if(param <= 1) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 0 ? false : true));

                    Entry* entry = vm -> globals.count ? findEntry(vm -> globals.entries, vm -> globals.capacity, name) : NULL;
                
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant global variable '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, true)));
                    } else {
                        RUNTIME_ERROR("Undefined variable '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if(param > 1 && param <= 3) {
                    // Local.
                    
                    uint32_t slot = readBytes(frame, param == 2 ? false : true);
                
                    PUSH(NUMBER_VAL(incdc(vm -> stack + frame -> slots + slot, true, true)));
                }
                else if(param > 3 && param <= 5) {
                    // Upvalue.

                    uint8_t slot = readBytes(frame, param == 4 ? false : true);

                    ObjUpvalue* upvalue = frame_closure -> upvalues[slot];
                
                    if(upvalue -> isConst) {
                        RUNTIME_ERROR("Attempt to decrement a constant upvalue!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    PUSH(NUMBER_VAL(incdc(upvalue -> location, true, true)));
                }
                else if(param > 5 && param <= 7) {
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 6u ? false : true));

                    Value value = *peek(vm, 0);
                
                    Table* fields;
                    
                    bool isDictionary = false;

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, true)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to decrement an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to decrement a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, true)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to decrement a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                else if(param == 8) {
                    Value value = *peek(vm, 1);
                
                    Value string = *peek(vm, 0);
                    
                    if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) || 
                    IS_BYTELIST(value)) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) {
                            RUNTIME_ERROR("Index is not convertable to number!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        int index = (int) idxval;
                        
                        if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass)) {
                            ObjList* list = NULL;

                            if(IS_LIST(value)) list = VALUE_LIST(value);
                            else {
                                ObjInstance* instance = VALUE_INSTANCE(value);

                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, listField, &valueContainer);

                                list = VALUE_LIST(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += list -> count;
                            
                            if(index + 1 > list -> count || index < 0) {
                                RUNTIME_ERROR("List index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Property
                            POP();    // String.
                            
                            PUSH(NUMBER_VAL(incdc(list -> values + index, true, true)));
                            
                            break;
                        }
                        else if(IS_BYTELIST(value)) {
                            ObjByteList* byteList = VALUE_BYTELIST(value);
                            
                            if(index < 0) 
                                index += byteList -> size;
                            
                            if(index + 1u > byteList -> size || index < 0) {
                                RUNTIME_ERROR("ByteList index out of bound!");
                            
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();
                            POP();
                            
                            if(byteList -> bytes[index] - 1 >= 0) 
                                byteList -> bytes[index]--;
                            
                            PUSH(NUMBER_VAL((double) byteList -> bytes[index]));
                            
                            break;
                        }
                    }
                    
                    Table* fields;
                    
                    bool isDictionary = false;

                    char* result = toString(vm, (Value* const) &string);
                    size_t size  = strlen(result);

                    vm -> bytesAllocated += size + 1u;

                    ObjString* name = TAKE_STRING(result, size, true);

                    ValueContainer valueContainer;
                    
                    if(IS_INSTANCE(value)) {
                        ObjInstance* instance = VALUE_INSTANCE(value);

                        if(instance -> klass == vmDictionaryClass) {
                            tableGet(&instance -> fields, dictionaryField, &valueContainer);

                            fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                            Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;

                            if(entry != NULL && entry -> key != NULL) {
                                if(entry -> valueContainer.isConst) {
                                    RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);

                                    return INTERPRET_RUNTIME_ERROR;
                                }

                                POP();   // Instance.

                                PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, true)));

                                break;
                            } else {
                                RUNTIME_ERROR("Attmept to decrement an undefined property '%s'!", name -> buffer);
                            }
                        }

                        fields = &instance -> fields;
                        
                        isDictionary = false;
                    }
                    else if(IS_DICTIONARY(value)) {
                        fields = &VALUE_DICTIONARY(value) -> fields;
                        
                        isDictionary = true;
                    } else {
                        RUNTIME_ERROR("Attempt to decrement a property of a non-(instance/dictionary) value!");

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = fields -> count ? findEntry(fields -> entries, fields -> capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, true)));
                    }
                    else if(!isDictionary) {
                        ObjInstance* instance = VALUE_INSTANCE(value);
                    
                        entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                        
                        if(entry != NULL && entry -> key != NULL) {
                            RUNTIME_ERROR("Attempt to decrement a static property through instance! Don't do that!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        } else {
                            RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    else {
                        RUNTIME_ERROR("Attempt to decrement an undefined property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    Value value = *peek(vm, 0);
                
                    if(!IS_CLASS(value)) {
                        RUNTIME_ERROR("Attempt to access static property from non-class! If it's an instance try using '.'!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    ObjClass* klass = VALUE_CLASS(value);
                    
                    ObjString* name = VALUE_STRING(readConstant(frame, frame_function, param == 9 ? false : true));
                    
                    Entry* entry = klass -> statics.count ? findEntry(klass -> statics.entries, klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        if(entry -> valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to decrement a constant static property '%s'!", name -> buffer);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        POP();    // Instance.
                        
                        PUSH(NUMBER_VAL(incdc(&entry -> valueContainer.value, true, true)));
                    } else {
                        RUNTIME_ERROR("Attempt to decrement an undefined static property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                break;
            }
            
            case OP_RECEIVE: {
                uint8_t type = READ_BYTE();

                Value value;

                char* str = get_string();
                
                if(str != NULL) {
                    if(type == 0) 
                        value = OBJECT_VAL(TAKE_STRING(str, strlen(str), true));        // The string will be freed automatically when VM is freed.
                    else if(type == 1) {
                        value = NUMBER_VAL(pstrtod(str));
                        
                        free(str);        // Free the string as it won't be freed by the VM any longer.
                    }
                    else if(type == 2) {
                        value = BOOL_VAL(!istrcmp(str, "true") ? true : false);
                        
                        free(str);
                    }
                } else value = NULL_VAL;

                PUSH(value);

                break;
            }
            
            case OP_SILENT_POP: POP(); break;
            case OP_DUP: PUSH(*peek(vm, 0)); break;
            
            case OP_BITWISE_AND: {
                Value value2 = POP();
                Value value1 = POP();
                
                BITWISE(value1, value2, &);
                
                break;
            }
            
            case OP_BITWISE_OR: {
                Value value2 = POP();
                Value value1 = POP();
                
                BITWISE(value1, value2, |);
                
                break;
            }

            // Right and Left shift operation is different from other
            // bitwise calculations.

            // Here, shiftee's bits actually gets moved.
            // So, let's say, I'm to use something like 10 << 24332
            // the result is unpredictable, as no data types (except BigInt
            // which is out of question here) which has this amount of bits.
            // So, shifting operations should be handled exceptionally.
            
            case OP_RIGHT_SHIFT: {
                Value value2 = POP();
                Value value1 = POP();

                double a, b;

                NumberData data = toNumber(vm, &value1);

                if(data.hadError == true) 
                    return INTERPRET_RUNTIME_ERROR;
                
                a = data.number;

                data = toNumber(vm, &value2);

                if(data.hadError == true) 
                    return INTERPRET_RUNTIME_ERROR;
                
                b = data.number;

                a = (isinf(a) || isnan(a)) ? 0 : a;
                b = (isinf(b) || isnan(b)) ? 0 : fabs(b);

                if(a > MAX_SAFE_INTEGER || b > MAX_SAFE_INTEGER || a < MIN_SAFE_INTEGER || b < MIN_SAFE_INTEGER) {
                    RUNTIME_ERROR("The provided number is out of bound to be converted to integer!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                if(b > 52) {
                    PUSH(NUMBER_VAL(0));

                    break;
                }

                long long ba = (long long) trunc(a);
                long long bb = (long long) trunc(b);

                PUSH(NUMBER_VAL((double) (ba >> bb)));
                
                break;
            }
            
            case OP_LEFT_SHIFT: {
                Value value2 = POP();
                Value value1 = POP();

                double a, b;

                NumberData data = toNumber(vm, &value1);

                if(data.hadError == true) 
                    return INTERPRET_RUNTIME_ERROR;
                
                a = data.number;

                data = toNumber(vm, &value2);

                if(data.hadError == true) 
                    return INTERPRET_RUNTIME_ERROR;
                
                b = data.number;

                a = (isinf(a) || isnan(a)) ? 0 : a;
                b = (isinf(b) || isnan(b)) ? 0 : fabs(b);

                if(a > MAX_SAFE_INTEGER || b > MAX_SAFE_INTEGER || a < MIN_SAFE_INTEGER || b < MIN_SAFE_INTEGER) {
                    RUNTIME_ERROR("The provided number is out of bound to be converted to integer!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                if(b > 52) {
                    PUSH(NUMBER_VAL(a > 0 ? INFINITY : -INFINITY));

                    break;
                }

                long long ba = (long long) trunc(a);
                long long bb = (long long) trunc(b);

                PUSH(NUMBER_VAL((double) (ba << bb)));
                
                break;
            }
            
            case OP_XOR: {
                Value value2 = POP();
                Value value1 = POP();
                
                BITWISE(value1, value2, ^);
                
                break;
            }
            
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm, vm -> stack + (vm -> stackTop - 1u));
                
                POP();
                
                break;
            }
            
            case OP_CLOSURE: {
                ObjFunction* function = VALUE_FUNCTION(readConstant(frame, frame_function, false));
                
                ObjClosure* closure = newClosure(vm, function);
                
                for(int i = 0; i < function -> upvalueCount; i++) {
                    bool isLocal   = readBytes(frame, false);
                    bool isConst   = readBytes(frame, false);
                    uint32_t index = readBytes(frame, true);
                    
                    if(isLocal) 
                        closure -> upvalues[i] = captureUpvalue(vm, vm -> stack + (frame -> slots + index), isConst);
                    else closure -> upvalues[i] = frame_closure -> upvalues[index];
                }
                
                PUSH(OBJECT_VAL(closure));
                
                break;
            }

            case OP_CLASS: {
                PUSH(OBJECT_VAL(newClass(vm, VALUE_STRING(readConstant(frame, frame_function, false)))));

                break;
            }

            case OP_CLASS_LONG: {
                PUSH(OBJECT_VAL(newClass(vm, VALUE_STRING(readConstant(frame, frame_function, true)))));

                break;
            }

            case OP_INHERIT: {
                if(!IS_CLASS(*peek(vm, 1))) {
                    RUNTIME_ERROR("Expected the superclass to be a class!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* superclass = VALUE_CLASS(*peek(vm, 1));
                ObjClass* subclass   = VALUE_CLASS(*peek(vm, 0));

                subclass -> superClass = superclass;

                tableInsertAll(&superclass -> methods, &subclass -> methods);

                POP();    // Subclass.

                break;
            }

            case OP_SUPER : {
                ObjString* name  = VALUE_STRING(readConstant(frame, frame_function, false));
                ObjClass* sclass = VALUE_CLASS(POP());

                if(!bindMethod(vm, sclass, name, 0)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                break;
            }

            case OP_DNM_SUPER : {
                ObjClass* sclass = VALUE_CLASS(POP());
                
                Value string = POP();

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += (size + 1u) * sizeof(char);

                ObjString* name = TAKE_STRING(result, size, true);

                if(!bindMethod(vm, sclass, name, 0)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                break;
            }

            case OP_SUPER_LONG: {
                ObjString* name  = VALUE_STRING(readConstant(frame, frame_function, true));
                ObjClass* sclass = VALUE_CLASS(POP());

                if(!bindMethod(vm, sclass, name, 0)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                break;
            }

            case OP_SUPER_INVOKE: {
                ObjClass* sclass = VALUE_CLASS(POP());

                ObjString* method = VALUE_STRING(readConstant(frame, frame_function, false));

                uint8_t argCount = READ_BYTE();

                if(!invokeFromClass(vm, sclass, method, argCount)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = vm -> frames + vm -> frameCount - 1u;

                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);

                break;
            }

            case OP_SUPER_INVOKE_LONG: {
                ObjClass* sclass = VALUE_CLASS(POP());

                ObjString* method = VALUE_STRING(readConstant(frame, frame_function, true));

                uint8_t argCount = READ_BYTE();

                if(!invokeFromClass(vm, sclass, method, argCount)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = vm -> frames + vm -> frameCount - 1u;

                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);

                break;
            }

            case OP_DNM_SUPER_INVOKE: {
                ObjClass* sclass = VALUE_CLASS(POP());
                
                Value string = POP();

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += (size + 1u) * sizeof(char);

                ObjString* method = TAKE_STRING(result, size, true);

                uint8_t argCount = READ_BYTE();

                if(!invokeFromClass(vm, sclass, method, argCount)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = vm -> frames + vm -> frameCount - 1u;

                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);

                break;
            }

            case OP_GET_PROPERTY: {
                Value value = *peek(vm, 0);

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                bool isDictionary = false;

                Table* fields = NULL;

                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        if(tableGet(fields, name, &valueContainer)) {
                            POP();

                            PUSH(valueContainer.value);

                            break;
                        }
                    }

                    fields = &instance -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                }

                if(fields != NULL && tableGet(fields, name, &valueContainer)) {
                    POP();

                    PUSH(valueContainer.value);

                    break;
                }

                WrapperStatus wrapperStatus = wrapperProperty(vm, value, name, 0);

                if(wrapperStatus.status == true) {
                    if(wrapperStatus.hadError == false) 
                        break;
                    else return INTERPRET_RUNTIME_ERROR;
                }

                if(fields == NULL) {
                    RUNTIME_ERROR("Attempt to get a property from a non-(instance/dictionary/wrapper-class) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(isDictionary) {
                    RUNTIME_ERROR("Undefined property '%s'!", name -> buffer);
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(!bindMethod(vm, VALUE_INSTANCE(value) -> klass, name, 0)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                break;
            }
            
            case OP_DNM_GET_PROPERTY: {
                Value value  = *peek(vm, 1);
                Value string = *peek(vm, 0);
                
                do {
                    if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) ||
                    IS_BYTELIST(value) ||
                    IS_STRING(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmStringClass)) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) 
                            break;

                        int index = (int) idxval;
                        
                        if(IS_LIST(value) || (IS_INSTANCE(value) &&
                        VALUE_INSTANCE(value) -> klass == vmListClass)) {
                            ObjList* list = NULL;

                            if(IS_LIST(value)) list = VALUE_LIST(value);
                            else {
                                ValueContainer valueContainer;

                                tableGet(&VALUE_INSTANCE(value) -> fields, listField, &valueContainer);

                                list = VALUE_LIST(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += list -> count;
                            
                            if(index + 1 > list -> count || index < 0) {
                                RUNTIME_ERROR("List index out of bound!");
                                
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Dynamic expression.
                            POP();    // List.
                            
                            PUSH(list -> values[index]);
                            
                            goto __continue_switch;
                        }
                        else if(IS_BYTELIST(value)) {
                            ObjByteList* byteList = VALUE_BYTELIST(value);
                            
                            if(index < 0) 
                                index += byteList -> size;
                            
                            if(index + 1u > byteList -> size || index < 0 ) {
                                RUNTIME_ERROR("ByteList index out of bound!");
                                
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Dynamic expression.
                            POP();    // List.
                            
                            PUSH(NUMBER_VAL((double) byteList -> bytes[index]));
                            
                            goto __continue_switch;
                        }
                        else if(IS_STRING(value) || (IS_INSTANCE(value) &&
                        VALUE_INSTANCE(value) -> klass == vmStringClass)) {
                            ObjString* string;

                            if(IS_STRING(value)) string = VALUE_STRING(value);
                            else {
                                ValueContainer valueContainer;

                                tableGet(&VALUE_INSTANCE(value) -> fields, stringField, &valueContainer);

                                string = VALUE_STRING(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += string -> length;
                            
                            if(index + 1 > string -> length || index < 0) {
                                RUNTIME_ERROR("String index out of bound!");
                                
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            POP();    // Dynamic expression.
                            POP();    // List.
                            
                            PUSH(OBJECT_VAL(copyString(vm, string -> buffer + index, 1u)));
                            
                            goto __continue_switch;
                        }
                    }
                } while(false);

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += (size + 1u) * sizeof(char);
                
                ObjString* name = TAKE_STRING(result, size, true);                
                
                bool isDictionary = false;
                
                Table* fields = NULL;
                
                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        if(tableGet(fields, name, &valueContainer)) {
                            POP();

                            PUSH(valueContainer.value);

                            break;
                        }
                    }

                    fields = &instance -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                }

                if(fields != NULL && tableGet(fields, name, &valueContainer)) {
                    POP();
                    POP();

                    PUSH(valueContainer.value);

                    break;
                }

                // Wrapper class property.

                WrapperStatus wrapperStatus = wrapperProperty(vm, value, name, 1);

                if(wrapperStatus.status == true) {
                    if(wrapperStatus.hadError == false) 
                        break;
                    else return INTERPRET_RUNTIME_ERROR;
                }

                if(fields == NULL) {
                    RUNTIME_ERROR("Attempt to get a property from a non-(instance/dictionary/wrapper-class) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                if(isDictionary) {
                    RUNTIME_ERROR("Undefined property '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(!bindMethod(vm, VALUE_INSTANCE(value) -> klass, name, 1)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                break;
            }

            case OP_GET_PROPERTY_LONG: {
                Value value = *peek(vm, 0);

                // Long constant.

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                bool isDictionary = false;

                Table* fields = NULL;

                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        if(tableGet(fields, name, &valueContainer)) {
                            POP();

                            PUSH(valueContainer.value);

                            break;
                        }
                    }

                    fields = &instance -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                }

                if(fields != NULL && tableGet(fields, name, &valueContainer)) {
                    POP();

                    PUSH(valueContainer.value);

                    break;
                }

                WrapperStatus wrapperStatus = wrapperProperty(vm, value, name, 0);

                if(wrapperStatus.status == true) {
                    if(wrapperStatus.hadError == false) 
                        break;
                    else return INTERPRET_RUNTIME_ERROR;
                }

                if(fields == NULL) {
                    RUNTIME_ERROR("Attempt to get a property from a non-(instance/dictionary/wrapper-class) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(isDictionary) {
                    RUNTIME_ERROR("Undefined property '%s'!", name -> buffer);
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(!bindMethod(vm, VALUE_INSTANCE(value) -> klass, name, 0)) 
                    return INTERPRET_RUNTIME_ERROR;
                
                break;
            }
            
            // Please don't mind the redundancy. That was the best idea I had that time.
            // Keeps the instance in the stack.
                        
            case OP_GET_PROPERTY_INST: {
                Value value = *peek(vm, 0);

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                bool isDictionary = false;
                
                Table* fields = NULL;

                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        // Keeping the instance.

                        if(tableGet(fields, name, &valueContainer)) {
                            if(valueContainer.isConst) {
                                RUNTIME_ERROR("Attempt to set a const property '%s'!", name -> buffer);

                                return INTERPRET_RUNTIME_ERROR;
                            }

                            PUSH(valueContainer.value);

                            break;
                        }
                    }

                    fields = &instance -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                } else {
                    RUNTIME_ERROR("Attempt to set a property to a non-instance/non-dictionary value!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                if(tableGet(fields, name, &valueContainer)) {
                    if(valueContainer.isConst) {
                        RUNTIME_ERROR("Attempt to set a const property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    // OP_GET_PROPERTY_INST keeps the instance. It's for very special use.
                    
                    PUSH(valueContainer.value);

                    break;
                }

                if(isDictionary) {
                    RUNTIME_ERROR("Attempt to set a property to a non-instance/non-dictionary value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(!bindMethod(vm, VALUE_INSTANCE(value) -> klass, name, 2)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                break;
            }
            
            case OP_DNM_GET_PROPERTY_INST: {
                Value value = *peek(vm, 1);
                
                Value string = *peek(vm, 0);
                
                do {
                    if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) || 
                    IS_BYTELIST(value)) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) {
                            RUNTIME_ERROR("Index is not convertable to number!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        int index = (int) idxval;
                        
                        if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass)) {
                            ObjList* list = NULL;

                            if(IS_LIST(value)) list = VALUE_LIST(value);
                            else {
                                ObjInstance* instance = VALUE_INSTANCE(value);

                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, listField, &valueContainer);

                                list = VALUE_LIST(valueContainer.value);
                            }
                            
                            if(index < 0) 
                                index += list -> count;
                            
                            if(index + 1 > list -> count || index < 0) {
                                RUNTIME_ERROR("List index out of bound!");
                                
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            PUSH(list -> values[index]);
                            
                            goto __continue_switch;
                        }
                        else if(IS_BYTELIST(value)) {
                            ObjByteList* byteList = VALUE_BYTELIST(value);
                            
                            if(index < 0) 
                                index += byteList -> size;
                            
                            if(index + 1u > byteList -> size || index < 0) {
                                RUNTIME_ERROR("ByteList index out of bound!");
                                
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            PUSH(NUMBER_VAL((double) byteList -> bytes[index]));
                            
                            goto __continue_switch;
                        }
                    }
                } while(false);

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += size + 1u;
                
                ObjString* name = TAKE_STRING(result, size, true);
                
                bool isDictionary = false;
                
                Table* fields;
                
                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        if(tableGet(fields, name, &valueContainer)) {
                            if(valueContainer.isConst) {
                                RUNTIME_ERROR("Attempt to set a const property '%s'!", name -> buffer);

                                return INTERPRET_RUNTIME_ERROR;
                            }

                            PUSH(valueContainer.value);

                            break;
                        }
                    }

                    fields = &instance -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                } else {
                    RUNTIME_ERROR("Attempt to set a property to a non-instance/non-dictionary/non-list value!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                WrapperStatus wrapperStatus = wrapperProperty(vm, value, name, 3);

                if(wrapperStatus.status == true) {
                    if(wrapperStatus.hadError == false) 
                        break;
                    else return INTERPRET_RUNTIME_ERROR;
                }

                if(tableGet(fields, name, &valueContainer)) {
                    if(valueContainer.isConst) {
                        RUNTIME_ERROR("Attempt to set a const property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    PUSH(valueContainer.value);

                    break;
                }

                if(isDictionary) {
                    RUNTIME_ERROR("Attempt to set a property to a non-(instance/dictionary) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(!bindMethod(vm, VALUE_INSTANCE(value) -> klass, name, 3)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                break;
            }

            case OP_GET_PROPERTY_INST_LONG: {
                Value value = *peek(vm, 0);

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                bool isDictionary = false;
                
                Table* fields = NULL;

                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        // Keeping the instance.

                        if(tableGet(fields, name, &valueContainer)) {
                            if(valueContainer.isConst) {
                                RUNTIME_ERROR("Attempt to set a const property '%s'!", name -> buffer);

                                return INTERPRET_RUNTIME_ERROR;
                            }

                            PUSH(valueContainer.value);

                            break;
                        }
                    }

                    fields = &instance -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                } else {
                    RUNTIME_ERROR("Attempt to set a property to a non-instance/non-dictionary value!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                if(tableGet(fields, name, &valueContainer)) {
                    if(valueContainer.isConst) {
                        RUNTIME_ERROR("Attempt to set a const property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    // OP_GET_PROPERTY_INST keeps the instance. It's for very special use.
                    
                    PUSH(valueContainer.value);

                    break;
                }

                if(isDictionary) {
                    RUNTIME_ERROR("Attempt to set a property to a non-instance/non-dictionary value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(!bindMethod(vm, VALUE_INSTANCE(value) -> klass, name, 2)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                break;
            }

            case OP_SET_PROPERTY: {
                Value value = *peek(vm, 1);

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));

                bool isConst = READ_BYTE();
                
                Table* fields;

                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        // Check whether the value exists. If it does, check whether it's
                        // constant.

                        if(tableGet(fields, name, &valueContainer) && valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to set a defined constant property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }

                        tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst } );

                        Value pushable = POP();

                        POP();    // Instance.

                        PUSH(pushable);

                        break;
                    }

                    fields = &instance -> fields;
                }
                else if(IS_DICTIONARY(value)) 
                    fields = &VALUE_DICTIONARY(value) -> fields;
                else {
                    RUNTIME_ERROR("Attempt to set a property to a non-(instance/dictionary) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if(tableGet(fields, name, &valueContainer) && valueContainer.isConst) {
                    RUNTIME_ERROR("Attempt to set a defined constant property '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }

                tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst } );

                Value pushable = POP();

                POP();    // Instance.

                PUSH(pushable);

                goto __run_gc;
            }
            
            case OP_DNM_SET_PROPERTY: {
                Value value = *peek(vm, 2);
                
                Value string = *peek(vm, 1);

                bool isConst = READ_BYTE();
                
                if(IS_LIST(value) || (IS_INSTANCE(value) && VALUE_INSTANCE(value) -> klass == vmListClass) || 
                IS_BYTELIST(value)) {
                    vmNumberData data = vmToNumber(vm, &string);
                        
                    if(data.hadError) 
                        return INTERPRET_RUNTIME_ERROR;
                    
                    double idxval = data.number;

                    if(!data.isRepresentable) {
                        RUNTIME_ERROR("Index is not convertable to number!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    int index = (int) idxval;
                    
                    if(IS_LIST(value) || (IS_INSTANCE(value) &&
                    VALUE_INSTANCE(value) -> klass == vmListClass)) {
                        ObjList* list = NULL;

                        if(IS_LIST(value)) list = VALUE_LIST(value);
                        else {
                            ObjInstance* instance = VALUE_INSTANCE(value);

                            ValueContainer valueContainer;

                            tableGet(&instance -> fields, listField, &valueContainer);

                            list = VALUE_LIST(valueContainer.value);
                        }
                        
                        if(index < 0) {
                            if(-index > list -> count) {
                                int newCount = -index;
                                
                                if(-index > list -> capacity) {
                                    int oldCapacity = list -> capacity;
                                    
                                    list -> capacity = newCount;
                                    list -> values   = GROW_ARRAY(Value, list -> values, oldCapacity, list -> capacity);
                                }
                            
                                for(register int i = 0; i < newCount - list -> count; i++) {
                                    if(i < list -> count) 
                                        list -> values[newCount - list -> count + i] = list -> values[i];
                                    
                                    list -> values[i] = NULL_VAL;
                                }
                                
                                list -> count = newCount;
                            }
                            
                            index += list -> count;
                        }
                        else if(index + 1 > list -> count) {
                            int newCount = index + 1u;
                            
                            if(index + 1 > list -> capacity) {
                                int oldCapacity = list -> capacity;
                                
                                list -> capacity = newCount;
                                list -> values   = GROW_ARRAY(Value, list -> values, oldCapacity, list -> capacity);
                            }
                            
                            for(register int i = list -> count; i < newCount; i++) 
                                list -> values[i] = NULL_VAL;
                            
                            list -> count = newCount;
                        }
                        
                        list -> values[index] = POP();
                        
                        POP();
                        POP();
                        
                        PUSH(list -> values[index]);
                        
                        goto __run_gc;
                    }
                    else if(IS_BYTELIST(value)) {
                        ObjByteList* byteList = VALUE_BYTELIST(value);
                        
                        if(index + 1u > byteList -> size) {
                            RUNTIME_ERROR("ByteList index out of bound!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        Value byte = POP();
                        
                        if(!IS_NUMBER(byte)) {
                            RUNTIME_ERROR("Expected a number to be assigned in bytelist!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        double number = VALUE_NUMBER(byte);
                        
                        if(number < 0 || number > 255) {
                            RUNTIME_ERROR("Assignable number should be in range from 0 to 255, found %g!", number);
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        byteList -> bytes[index] = (uint8_t) number;
                        
                        POP();
                        POP();
                        
                        PUSH(byte);
                        
                        // ByteList only supports numbers. So no need to call the GC.
                        
                        break;
                    }
                }

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += size + 1u;

                ObjString* name = TAKE_STRING(result, size, true);
                
                ValueContainer valueContainer;
                
                Table* fields;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);
                        
                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        // Check whether the value exists. If it does, check whether it's
                        // constant.

                        if(tableGet(fields, name, &valueContainer) && valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to set a defined constant property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }

                        tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst } );

                        Value pushable = POP();

                        POP();    // Property string.
                        POP();    // Instance.

                        PUSH(pushable);

                        break;
                    }

                    fields = &instance -> fields;
                }
                else if(IS_DICTIONARY(value)) 
                    fields = &VALUE_DICTIONARY(value) -> fields;
                else {
                    RUNTIME_ERROR("Attempt to set a property to a non-instance/non-dictionary value!");

                    return INTERPRET_RUNTIME_ERROR;
                }

                if(tableGet(fields, name, &valueContainer) && valueContainer.isConst) {
                    RUNTIME_ERROR("Attempt to set a defined constant property '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }

                tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst });
                
                Value pushable = POP();
                
                POP();    // Property string.
                POP();    // Instance.
                
                PUSH(pushable);
                
                goto __run_gc;
            }

            case OP_SET_PROPERTY_LONG: {
                Value value = *peek(vm, 1);

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));

                bool isConst = READ_BYTE();
                
                Table* fields;

                ValueContainer valueContainer;
                
                if(IS_INSTANCE(value)) {
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &valueContainer);

                        fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                        // Check whether the value exists. If it does, check whether it's
                        // constant.

                        if(tableGet(fields, name, &valueContainer) && valueContainer.isConst) {
                            RUNTIME_ERROR("Attempt to set a defined constant property '%s'!", name -> buffer);

                            return INTERPRET_RUNTIME_ERROR;
                        }

                        tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst } );

                        Value pushable = POP();

                        POP();    // Instance.

                        PUSH(pushable);

                        break;
                    }

                    fields = &instance -> fields;
                }
                else if(IS_DICTIONARY(value)) 
                    fields = &VALUE_DICTIONARY(value) -> fields;
                else {
                    RUNTIME_ERROR("Attempt to set a property to a non-(instance/dictionary) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if(tableGet(fields, name, &valueContainer) && valueContainer.isConst) {
                        RUNTIME_ERROR("Attempt to set a defined constant property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                }

                tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst } );

                Value pushable = POP();

                POP();    // Instance.

                PUSH(pushable);

                goto __run_gc;
            }
            
            case OP_DICTIONARY: 
                PUSH(OBJECT_VAL(newDictionary(vm)));
                break;
            
            case OP_ADD_DICTIONARY: {
                Value value = *peek(vm, 1);
                
                Table* fields = &VALUE_DICTIONARY(value) -> fields;

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));

                bool isConst = READ_BYTE();

                tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst });

                POP();

                break;
            }
            
            case OP_ADD_DICTIONARY_LONG: {
                Value value = *peek(vm, 1);
                
                Table* fields = &VALUE_DICTIONARY(value) -> fields;

                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));

                bool isConst = READ_BYTE();

                tableInsert(fields, name, (ValueContainer) { *peek(vm, 0), isConst });

                POP();

                break;
            }
            
            case OP_METHOD: {
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                uint8_t isStatic = readBytes(frame, false);
                uint8_t isConst  = readBytes(frame, false);
                
                defineMethod(vm, name, isConst, isStatic);
                
                break;
            }
            
            case OP_METHOD_LONG: {
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                uint8_t isStatic = readBytes(frame, false);
                uint8_t isConst  = readBytes(frame, false);
                
                defineMethod(vm, name, isConst, isStatic);
                
                break;
            }
            
            case OP_DELETE_PROPERTY: {
                Value value = *peek(vm, 0);
                
                Table* fields;
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                bool isStatic = readBytes(frame, false);
                
                if(isStatic) {
                    if(IS_CLASS(value)) {
                        ObjClass* klass = VALUE_CLASS(value);
                        
                        if(!tableDelete(&klass -> statics, name)) {
                            RUNTIME_ERROR("Attempt to delete undefined static property '%s'!", name -> buffer);
                    
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        goto __run_gc;
                    } else {
                        RUNTIME_ERROR("Attempt to delete a static property from non-class value!");
                    
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                bool isDictionary = false;
                
                if(IS_INSTANCE(value)) {
                    fields = &VALUE_INSTANCE(value) -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                } else {
                    RUNTIME_ERROR("Attempt to delete a property of a non-(instance/dictionary) value!");
            
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if(!tableDelete(fields, name)) {
                    RUNTIME_ERROR("Attempt to delete undefined property '%s'!", name -> buffer);
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if(!isDictionary) {
                    ObjInstance* instance = VALUE_INSTANCE(value);
                    
                    Entry* entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        RUNTIME_ERROR("Attempt to delete a static property through instance! Don't do that!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                POP();
                
                goto __run_gc;
            }
            
            case OP_DNM_DELETE_PROPERTY: {
                Value value = *peek(vm, 1);
                
                Table* fields;
                
                bool isDictionary = false;
                
                if(IS_INSTANCE(value)) {
                    fields = &VALUE_INSTANCE(value) -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                } else {
                    RUNTIME_ERROR("Attempt to delete a property of a non-(instance/dictionary) value!");

                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Value string = *peek(vm, 0);

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += size + 1u;

                ObjString* name = TAKE_STRING(result, size, true);
                
                if(tableDelete(fields, name)) {
                    POP();    // Generated expression.
                    POP();    // Instance.
                    
                    goto __run_gc;
                }
                
                if(!isDictionary) {
                    ObjInstance* instance = VALUE_INSTANCE(value);
                    
                    Entry* entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        RUNTIME_ERROR("Attempt to delete a static property through instance! Don't do that!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                POP();    // Generated expression.
                POP();    // Instance.
                
                goto __run_gc;
            }
            
            case OP_DELETE_PROPERTY_LONG: {
                Value value = *peek(vm, 0);
                
                Table* fields;
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                bool isStatic = readBytes(frame, false);
                
                if(isStatic) {
                    if(IS_CLASS(value)) {
                        ObjClass* klass = VALUE_CLASS(value);
                        
                        if(!tableDelete(&klass -> statics, name)) {
                            RUNTIME_ERROR("Attempt to delete undefined static property '%s'!", name -> buffer);
                    
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        break;
                    } else {
                        RUNTIME_ERROR("Attempt to delete a static property from non-class value!");
                    
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                bool isDictionary = false;
                
                if(IS_INSTANCE(value)) {
                    fields = &VALUE_INSTANCE(value) -> fields;
                    
                    isDictionary = false;
                }
                else if(IS_DICTIONARY(value)) {
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    isDictionary = true;
                } else {
                    RUNTIME_ERROR("Attempt to delete a property of a non-(instance/dictionary) value!");
            
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if(!tableDelete(fields, name)) {
                    RUNTIME_ERROR("Attempt to delete undefined property '%s'!", name -> buffer);
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if(!isDictionary) {
                    ObjInstance* instance = VALUE_INSTANCE(value);
                    
                    Entry* entry = instance -> klass -> statics.count ? findEntry(instance -> klass -> statics.entries, instance -> klass -> statics.capacity, name) : NULL;
                    
                    if(entry != NULL && entry -> key != NULL) {
                        RUNTIME_ERROR("Attempt to delete a static property through instance! Don't do that!");
                        
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                POP();
                
                goto __run_gc;
            }

            // Invokes a function from any instance/dictionary/wrapper-class.
            // ins.call(); -> tights into 4 bytecodes.
            //  ----
            //     --> OP_GET_[GLOBAL/LOCAL/UPVALUE]
            //  --> OP_GET_PROPERTY        => Creates a bind method of 'call'.
            //  --> OP_CALL/OP_CALL_[1-20]  => Calls the method/function.
            //  ----
            // Which can lead to performance issues.
            // So OP_INVOKE is a direct approach to call a method/function from
            // an instance/dictionary/wrapper-class.
            //  ----
            //  --> OP_GET_[GLOBAL/LOCAL/UPVALUE]
            //  --> OP_INVOKE    => Boom! Done!
            // In benchmark it is likely to have increased the performance almost
            // by 2 times.
            
            case OP_INVOKE: {
                ObjString* method = VALUE_STRING(readConstant(frame, frame_function, false));
                
                bool isStatic = readBytes(frame, false);
                
                uint8_t argCount = readBytes(frame, false);
                
                Value value = *peek(vm, argCount);
                
                uint8_t state = 0u;
                
                do {
                    if(!isStatic) {
                        if(IS_INSTANCE(value)) {
                            ObjInstance* instance = VALUE_INSTANCE(value);

                            if(instance -> klass == vmDictionaryClass) {
                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, dictionaryField, &valueContainer);

                                Table* fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                                if(tableGet(fields, method, &valueContainer)) {
                                    vm -> stack[vm -> stackTop - argCount - 1u] = valueContainer.value;
                                    
                                    if(!callValue(vm, valueContainer.value, argCount)) 
                                        return INTERPRET_RUNTIME_ERROR;
                                    
                                    break;
                                }
                            }

                            state = 0u;
                        }
                        else if(IS_DICTIONARY(value)) 
                            state = 1u;
                        else state = 2u;
                    }

                    InvokeData data = invoke(vm, method, argCount, state, isStatic);
                    
                    if(data.status == true) {
                        frame = vm -> frames + vm -> frameCount - 1u;
                        
                        frame_function = getFunction(frame -> function);
                        frame_closure  = getClosure(frame -> function);

                        break;
                    }
                    
                    if(!isStatic) {
                        WrapperStatus wrapperStatus = wrapperInvoke(vm, value, method, argCount);

                        if(wrapperStatus.status == true) {
                            if(wrapperStatus.hadError == false) 
                                break;
                            else return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    
                    if(state == 2u) 
                        RUNTIME_ERROR("Attempt to access method from non-(instance/dictionary/wrapper-class) value!");
                    else if(data.message != NULL) 
                        RUNTIME_ERROR(data.message, method -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                } while(false);
                
                break;
            }
            
            case OP_INVOKE_LONG: {
                ObjString* method = VALUE_STRING(readConstant(frame, frame_function, true));
                
                bool isStatic = readBytes(frame, false);
                
                uint8_t argCount = readBytes(frame, false);
                
                Value value = *peek(vm, argCount);
                
                uint8_t state = 0u;
                
                do {
                    if(!isStatic) {
                        if(IS_INSTANCE(value)) {
                            ObjInstance* instance = VALUE_INSTANCE(value);

                            if(instance -> klass == vmDictionaryClass) {
                                ValueContainer valueContainer;

                                tableGet(&instance -> fields, dictionaryField, &valueContainer);

                                Table* fields = &VALUE_DICTIONARY(valueContainer.value) -> fields;

                                if(tableGet(fields, method, &valueContainer)) {
                                    vm -> stack[vm -> stackTop - argCount - 1u] = valueContainer.value;
                                    
                                    if(!callValue(vm, valueContainer.value, argCount)) 
                                        return INTERPRET_RUNTIME_ERROR;
                                    
                                    break;
                                }
                            }

                            state = 0u;
                        }
                        else if(IS_DICTIONARY(value)) 
                            state = 1u;
                        else state = 2u;
                    }

                    InvokeData data = invoke(vm, method, argCount, state, isStatic);
                    
                    if(data.status == true) {
                        frame = vm -> frames + vm -> frameCount - 1u;
                        
                        frame_function = getFunction(frame -> function);
                        frame_closure  = getClosure(frame -> function);

                        break;
                    }
                    
                    if(!isStatic) {
                        WrapperStatus wrapperStatus = wrapperInvoke(vm, value, method, argCount);

                        if(wrapperStatus.status == true) {
                            if(wrapperStatus.hadError == false) 
                                break;
                            else return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    
                    if(state == 2u) 
                        RUNTIME_ERROR("Attempt to access method from non-(instance/dictionary/wrapper-class) value!");
                    else if(data.message != NULL) 
                        RUNTIME_ERROR(data.message, method -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                } while(false);
                
                break;
            }
            
            case OP_DNM_INVOKE_START: {
                Value value = *peek(vm, 1);
                
                Value string = POP();
                
                do {
                    if(IS_LIST(value) || (IS_INSTANCE(value) &&
                    !strcmp(VALUE_INSTANCE(value) -> klass -> name -> buffer, "List"))) {
                        vmNumberData data = vmToNumber(vm, &string);
                        
                        if(data.hadError) 
                            return INTERPRET_RUNTIME_ERROR;
                        
                        double idxval = data.number;

                        if(!data.isRepresentable) 
                            break;

                        int index = (int) idxval;
                        
                        ObjList* list;

                        if(IS_LIST(value)) list = VALUE_LIST(value);
                        else {
                            ValueContainer valueContainer;

                            tableGet(&VALUE_INSTANCE(value) -> fields, listField, &valueContainer);

                            list = VALUE_LIST(valueContainer.value);
                        }

                        if(index < 0) index += list -> count;
                        
                        if(index + 1 > list -> count) {
                            RUNTIME_ERROR("List index out of bound!");
                            
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        invokeTemp = list -> values[index];
                        
                        goto __continue_switch;
                    }
                } while(false);

                char* result = toString(vm, (Value* const) &string);
                size_t size  = strlen(result);

                vm -> bytesAllocated += (size + 1u) * sizeof(char);
                
                ObjString* name = TAKE_STRING(result, size, true);
                
                ValueContainer field;

                Table* fields = NULL;
                
                uint8_t state = 0u;
                
                if(IS_INSTANCE(value)) {    // Instance.
                    ObjInstance* instance = VALUE_INSTANCE(value);

                    if(instance -> klass == vmDictionaryClass) {
                        tableGet(&instance -> fields, dictionaryField, &field);

                        fields = &VALUE_DICTIONARY(field.value) -> fields;

                        if(tableGet(fields, name, &field)) 
                            invokeTemp = field.value;
                        
                        break;
                    }

                    fields = &instance -> fields;
                    
                    state = 0u;
                }
                else if(IS_DICTIONARY(value)) {   // Dictionary.
                    fields = &VALUE_DICTIONARY(value) -> fields;
                    
                    state = 1u;
                } else state = 2u;
                
                if(fields != NULL && tableGet(fields, name, &field)) {
                    invokeTemp = field.value;
                    
                    break;
                }
                
                if(state == 0u && tableGet(&VALUE_INSTANCE(value) -> klass -> statics, name, &field)) {
                    invokeTemp = field.value;
                    
                    break;
                }
    
                if(state == 0u && tableGet(&VALUE_INSTANCE(value) -> klass -> methods, name, &field)) {
                    invokeTemp = field.value;

                    break;
                }

                ObjClass* klass = fetchClass(value);

                // If it's a wrapper class.

                if(klass != NULL && (tableGet(&klass -> methods, name, &field) || tableGet(&klass -> statics, name, &field))) {
                    invokeTemp = field.value;

                    break;
                }
                
                if(state == 0u) {
                    RUNTIME_ERROR("Undefined property '%s'!", name -> buffer);
        
                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(state == 1u) {
                    RUNTIME_ERROR("Undefined dictionary property '%s'!", name -> buffer);
        
                    return INTERPRET_RUNTIME_ERROR;
                }
                else if(klass != NULL) {
                    RUNTIME_ERROR("Undefined wrapper property '%s'!", name -> buffer);
                    
                    return INTERPRET_RUNTIME_ERROR;
                } else {
                    RUNTIME_ERROR("Attempt to access a method from non-(instance/dictionary/wrapper-class) value!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }

                invokeTemp = field.value;
                
                break;
            }
            
            case OP_DNM_INVOKE_END: {
                if(!callValue(vm, invokeTemp, readBytes(frame, false))) 
                    return INTERPRET_RUNTIME_ERROR;
                
                frame = vm -> frames + vm -> frameCount - 1;
                
                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);
                
                break;
            }
            
            case OP_LIST: 
                PUSH(OBJECT_VAL(newList(vm)));
                break;
            
            case OP_ADD_LIST: {
                ObjList* list = VALUE_LIST(*peek(vm, 1));
                
                if(list -> count + 1 > list -> capacity) {
                    int oldCapacity = list -> capacity;
                    
                    list -> capacity = GROW_CAPACITY(list -> capacity);
                    list -> values   = GROW_ARRAY(Value, list -> values, oldCapacity, list -> capacity);
                }
                
                Value value = POP();
                
                list -> values[list -> count++] = value;
                
                break;
            }
            
            // Adds static properties while constructing the class.
            // No need to call the GC.
            
            case OP_SET_STATIC: {
                ObjClass* klass = VALUE_CLASS(*peek(vm, 1));
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));

                tableInsert(&klass -> statics, name, (ValueContainer) { *peek(vm, 0), READ_BYTE() });
                
                POP();
                
                break;
            }
            
            case OP_SET_STATIC_LONG: {
                ObjClass* klass = VALUE_CLASS(*peek(vm, 1));
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                tableInsert(&klass -> statics, name, (ValueContainer) { *peek(vm, 0), READ_BYTE() });
                
                POP();
                
                break;
            }
            
            case OP_GET_STATIC_PROPERTY: {
                if(!IS_CLASS(*peek(vm, 0))) {
                    RUNTIME_ERROR("Attempt to access static property from a non-class value! If it's an instance try using '.'!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* klass = VALUE_CLASS(*peek(vm, 0));
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                ValueContainer sttc;
                
                if(tableGet(&klass -> statics, name, &sttc)) {
                    POP();
                    
                    PUSH(sttc.value);
                    
                    break;
                }
                
                RUNTIME_ERROR("Undefined static property '%s'!", name -> buffer);
                
                return INTERPRET_RUNTIME_ERROR;
            }
            
            case OP_GET_STATIC_PROPERTY_LONG: {
                if(!IS_CLASS(*peek(vm, 0))) {
                    RUNTIME_ERROR("Attempt to access static property from a non-class value! If it's an instance try using '.'!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* klass = VALUE_CLASS(*peek(vm, 0));
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                ValueContainer sttc;
                
                if(tableGet(&klass -> statics, name, &sttc)) {
                    POP();
                    
                    PUSH(sttc.value);
                    
                    break;
                }
                
                RUNTIME_ERROR("Undefined static property '%s'!", name -> buffer);
                
                return INTERPRET_RUNTIME_ERROR;
            }
            
            case OP_GET_STATIC_PROPERTY_INST: {
                if(!IS_CLASS(*peek(vm, 0))) {
                    RUNTIME_ERROR("Attempt to access static property from a non-class value! If it's an instance try using '.'!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* klass = VALUE_CLASS(*peek(vm, 0));
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));
                
                ValueContainer sttc;
                
                if(tableGet(&klass -> statics, name, &sttc)) {
                    if(sttc.isConst) {
                        RUNTIME_ERROR("Attempt to set a defined static property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    PUSH(sttc.value);
                    
                    break;
                }
                
                RUNTIME_ERROR("Undefined static property '%s'!", name -> buffer);
                
                return INTERPRET_RUNTIME_ERROR;
            }
            
            case OP_GET_STATIC_PROPERTY_INST_LONG: {
                if(!IS_CLASS(*peek(vm, 0))) {
                    RUNTIME_ERROR("Attempt to access static property from a non-class value! If it's an instance try using '.'!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* klass = VALUE_CLASS(*peek(vm, 0));
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));
                
                ValueContainer sttc;
                
                if(tableGet(&klass -> statics, name, &sttc)) {
                    if(sttc.isConst) {
                        RUNTIME_ERROR("Attempt to set a defined static property '%s'!", name -> buffer);

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    PUSH(sttc.value);
                    
                    break;
                }
                
                RUNTIME_ERROR("Undefined static property '%s'!", name -> buffer);
                
                return INTERPRET_RUNTIME_ERROR;
            }
            
            case OP_SET_STATIC_PROPERTY: {
                Value value = *peek(vm, 1);
                
                if(!IS_CLASS(value)) {
                    RUNTIME_ERROR("Attempt to access static property from a non-class value! If it's an instance try using '.'!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* klass = VALUE_CLASS(value);
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, false));

                bool isConst = READ_BYTE();
                
                ValueContainer sttc;

                if(tableGet(&klass -> statics, name, &sttc) && sttc.isConst) {
                    RUNTIME_ERROR("Attempt to set a defined const static property '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }
                
                tableInsert(&klass -> statics, name, (ValueContainer) { *peek(vm, 0), isConst });
                
                value = POP();
                
                POP();
                
                PUSH(value);
                
                goto __run_gc;
            }
            
            case OP_SET_STATIC_PROPERTY_LONG: {
                Value value = *peek(vm, 1);
                
                if(!IS_CLASS(value)) {
                    RUNTIME_ERROR("Attempt to access static property from a non-class value! If it's an instance try using '.'!");
                    
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* klass = VALUE_CLASS(value);
                
                ObjString* name = VALUE_STRING(readConstant(frame, frame_function, true));

                bool isConst = READ_BYTE();
                
                ValueContainer sttc;

                if(tableGet(&klass -> statics, name, &sttc) && sttc.isConst) {
                    RUNTIME_ERROR("Attempt to set a defined const static property '%s'!", name -> buffer);

                    return INTERPRET_RUNTIME_ERROR;
                }
                
                tableInsert(&klass -> statics, name, (ValueContainer) { *peek(vm, 0), isConst });
                
                value = POP();
                
                POP();
                
                PUSH(value);
                
                goto __run_gc;
            }
            
            case OP_RETURN: {
                Value result = POP();
                
                closeUpvalues(vm, vm -> stack + (frame -> slots));
                
                vm -> frameCount--;
                
                if(vm -> frameCount == 0) {
                    POP();

                    goto __run_nested_1;
                }
                
                vm -> stackTop = frame -> slots;
                
                PUSH(result);
                
                frame = vm -> frames + (vm -> frameCount - 1u);
                
                frame_function = getFunction(frame -> function);
                frame_closure  = getClosure(frame -> function);

                if(vm -> frameCount + 1u == currentFrameCount) 
                    goto __run_nested_1;
                else goto __run_gc;
            }
        }
    }

    goto __run_nested_2;

    __run_nested_1: 
        if(vm -> bytesAllocated > vm -> nextGC) 
            garbageCollector();

    __run_nested_2: 
        return INTERPRET_OK;
}

#undef READ_BYTE

InterpretResult interpret(VM* vm, const char* source, bool included) {
    ObjFunction* function = compile(vm, source, included);
    
    if(function == NULL) 
        return INTERPRET_COMPILATION_ERROR;
    
    PUSH(OBJECT_VAL(function));
    
    if(!call(vm, (Obj*) function, 0)) 
        return INTERPRET_RUNTIME_ERROR;

    return run(vm);
}

void resetStack(VM* vm) {
    vm -> stackTop   = 0;
    vm -> frameCount = 0u;
}

void stack_push(VM* vm, Value value) {
    // Weird but optimized.
    
    *(vm -> stack + (vm -> stackTop++)) = value;
}

Value stack_pop(VM* vm) {
    return *(vm -> stack + (--vm -> stackTop));
}

int __printf(const char* format, ...) {
    va_list v;
    
    va_start(v, format);
    
    int wrote = -1;
    
    if(vm_stdout -> file != NULL) 
        wrote = vfprintf(vm_stdout -> file, format, v);
    
    va_end(v);
    
    return wrote;
}
