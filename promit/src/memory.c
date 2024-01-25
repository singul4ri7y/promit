#include "memory.h"
#include "object.h"
#include "compiler.h"

#define GC_HEAP_GROW_FACTOR 2u

VM* currentVM = NULL;

void setMemoryVM(VM* vm) {
    if(currentVM == NULL) 
        currentVM = vm;
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    // Keep track of the bytes allcoated.

    void* result = NULL;
    
    if(newSize == 0u) {
        free(pointer);
        goto __return_reallocate_1;
    }

    result = realloc(pointer, newSize);
    
    // If reallocation fails, don't count size change. Return NULL.

    if(result == NULL) 
        goto __return_reallocate_2;

    __return_reallocate_1: 
        currentVM -> bytesAllocated += newSize - oldSize;
    
    __return_reallocate_2: 
        return result;
}

void freeObject(Obj* obj) {
    switch(obj -> type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) obj;

            if(string -> heapAllocated) 
                FREE_ARRAY(char, string -> buffer, string -> length + 1u);
            else string -> buffer = NULL;

            FREE(ObjString, string);

            break;
        }
        
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*) obj;

            freeChunk(&function -> chunk);

            FREE(ObjFunction, function);
            
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) obj;
            
            FREE_ARRAY(ObjUpvalue*, closure -> upvalues, closure -> upvalueCount);

            FREE(ObjClosure, closure);

            break;
        }
        
        case OBJ_NATIVE: {
            FREE(ObjNative, obj);
            
            break;
        }
        
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, obj);
            
            break;
        }

        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*) obj;
            
            freeTable(&klass -> methods);
            freeTable(&klass -> statics);

            FREE(ObjClass, klass);

            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*) obj;

            freeTable(&instance -> fields);

            FREE(ObjInstance, instance);

            break;
        }
        
        case OBJ_BOUND_METHOD: {
            FREE(ObjBoundMethod, obj);
            
            break;
        }
        
        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*) obj;
            
            freeTable(&dictionary -> fields);
            
            FREE(ObjDictionary, dictionary);
            
            break;
        }
        
        case OBJ_LIST: {
            ObjList* list = (ObjList*) obj;
            
            FREE_ARRAY(Value, list -> values, list -> capacity);
            
            FREE(ObjList, list);
            
            break;
        }
        
        case OBJ_FILE: {
            // Close the file if opened.

            ObjFile* file = (ObjFile*) obj;

            if(file -> file != NULL) {
                fclose(file -> file);

                file -> file = NULL;
            }

            FREE(ObjFile, file);
            
            break;
        }
        
        // Use GROW_ARRAY and FREE_ARRAY.

        case OBJ_BYTELIST: {
            ObjByteList* byteList = (ObjByteList*) obj;
            
            free(byteList -> bytes);
            
            FREE(ObjByteList, byteList);
            
            break;
        }
    }
}

void freeObjects() {
    Obj* obj = currentVM -> objects;

    while(obj != NULL) {
        Obj* next = obj -> next;
        
        freeObject(obj);
        
        obj = next;
    }
}

void markObject(Obj* object) {
    if(object == NULL) return;

    if(object -> isMarked == true) 
        return;

    // Mark the object.

    object -> isMarked = true;

    if(currentVM -> grayCapacity < currentVM -> grayCount + 1) {
        currentVM -> grayCapacity = GROW_CAPACITY(currentVM -> grayCapacity);
        currentVM -> grayStack    = (Obj**) realloc(currentVM -> grayStack, currentVM -> grayCapacity * sizeof(Obj*));
        
        if(currentVM -> grayStack == NULL) {
            runtimeError(currentVM, "Garbage Collection failed! Failed to allocate gray stack for garbage collector!");
            exit(EXIT_FAILURE);
        }
    }

    // Add the obj to the gray stack for further reference tracing.
    
    currentVM -> grayStack[currentVM -> grayCount++] = object;
}

void markValue(Value value) {
    if(IS_OBJECT(value)) 
        markObject(VALUE_OBJECT(value));
}

static void markArray(ValueArray* valueArray) {
    for(register int i = 0; i < valueArray -> count; i++) 
        markValue(valueArray -> values[i]);
}

static void markRoots() {
    // First the stack.
    
    for(register int i = 0; i < currentVM -> stackTop; i++) 
        markValue(currentVM -> stack[i]);
    
    // The globals.
    
    markTable(&currentVM -> globals);

    // Frames are also objects.
    
    for(register short i = 0; i < currentVM -> frameCount; i++) 
        markObject((Obj*) currentVM -> frames[i].function);

    // Upvalues.

    for(register ObjUpvalue* upvalue = currentVM -> openUpvalues; upvalue != NULL; upvalue = upvalue -> next) 
            markObject((Obj*) upvalue);
}

static void blackenObject(Obj* object) {
    switch(object -> type) {
        case OBJ_NATIVE:
        case OBJ_FILE: 
        case OBJ_BYTELIST: 
        case OBJ_STRING: 
            break;
        
        case OBJ_UPVALUE: 
            markValue(((ObjUpvalue*) object) -> closed);
            break;
        
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*) object;
            
            markObject((Obj*) function -> name);
            markArray(&function -> chunk.constants);
            
            break;
        }

        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* method = (ObjBoundMethod*) object;

            markObject((Obj*) method -> function);

            break;
        }
        
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) object;
            
            markObject((Obj*) closure -> function);
            
            for(register int i = 0u; i < closure -> upvalueCount; i++) 
                markObject((Obj*) closure -> upvalues[i]);
            
            break;
        }

        case OBJ_LIST: {
            ObjList* list = (ObjList*) object;

            for(register int i = 0u; i < list -> count; i++) 
                markValue(list -> values[i]);

            break;
        }

        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*) object;

            markTable(&dictionary -> fields);

            break;
        }
        
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*) object;
            
            markObject((Obj*) klass -> name);
            markTable(&klass -> methods);
            markTable(&klass -> statics);
            
            break;
        }
        
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*) object;
            
            markObject((Obj*) instance -> klass);
            markTable(&instance -> fields);
            
            break;
        }
    }
}

static void traceReferences() {
    // Trace the references of the roots to mark furthermore reachable objects.
    
    while(currentVM -> grayCount > 0) 
        blackenObject(currentVM -> grayStack[--currentVM -> grayCount]);
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object   = currentVM -> objects;
    
    while(object != NULL) {
        if(object -> isMarked == true) {
            // Unmark the object.
            object -> isMarked = false;
            
            previous = object;
            object = object -> next;
        } else {
            Obj* unreachable = object;
            object = object -> next;
            
            if(previous != NULL) 
                previous -> next = object;
            else currentVM -> objects = object;
            
            freeObject(unreachable);
        }
    }
}

// From lib.c

extern void gcLibIgnore();

void garbageCollector() {
    // Mark-sweep garbage collector.
    // First mark the roots and add them to gray stack for further references.

    markRoots();
    markCompilerRoots();
    gcLibIgnore();

    // Then trace the references of the roots in the gray stack.
    
    traceReferences();

    // Remove the unmarked strings from the global string table. String interning stuffs.

    tableRemoveWhite(&currentVM -> strings);

    sweep();        // Mop the floor.

    // Update the next GC.
    
    currentVM -> nextGC = currentVM -> bytesAllocated * GC_HEAP_GROW_FACTOR;
}
