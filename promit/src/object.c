#include "object.h"
#include "memory.h"

#include <string.h>

#define ALLOCATE_OBJECT(vm, type, objType) (type*) allocateObject(vm, sizeof(type), objType)

bool isObjType(Value value, ObjType type) {
	return IS_OBJECT(value) && OBJ_TYPE(value) == type;
}

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
	Obj* object = (Obj*) reallocate(NULL, 0u, size);

	object -> type     = type;
	object -> isMarked = false;
	
	// Crating a object linked list in order to track the pointers and free it later on.
	
	object -> next   = vm -> objects;
	vm -> objects = object;

	return object;
}

static uint32_t hashString(const char* buffer, int length) {
	register uint32_t hash = 2166136261u;

	for (register int i = 0; i < length; i++) {
		hash ^= (uint8_t) buffer[i];
		hash *= 16777619;
	}

	return hash;
}

static ObjString* allocateString(VM* vm, char* buffer, size_t length, bool heapAllocated, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJECT(vm, ObjString, OBJ_STRING);

	string -> buffer        = buffer;
	string -> length        = length;
	string -> heapAllocated = heapAllocated;
	string -> hash          = hash;
	
	ValueContainer valueContainer;
	
	valueContainer.isConst = false;
	valueContainer.value   = NULL_VAL;
	
	tableInsert(&vm -> strings, string, valueContainer);

	return string;
}

ObjString* takeString(VM* vm, char* buffer, size_t length, bool heapAllocated) {
	uint32_t hash = hashString(buffer, length);
	
	ObjString* interned = tableFindString(&vm -> strings, buffer, length, hash);

	if(interned != NULL) {
		if(heapAllocated) 
			FREE_ARRAY(char, buffer, length + 1);
		else buffer = NULL;
		
		return interned;
	}

	return allocateString(vm, buffer, length, heapAllocated, hash);
}

ObjString* copyString(VM* vm, const char* start, size_t length) {
	uint32_t hash = hashString(start, length);

	char* buffer = ALLOCATE(char, length + 1);

	register size_t len, stride;
	
	len = stride = 0u;
	
	char ch;
	
	for(register size_t i = 0; i < length; i++) {
		ch = start[i];
		
		if(ch == '\\') {
			switch(start[i + 1]) {
				case 'n': ch = '\n'; break;
				case 'f': ch = '\f'; break;
				case 'r': ch = '\r'; break;
				case 'b': ch = '\b'; break;
				case 'v': ch = '\v'; break;
				case 't': ch = '\t'; break;
				case '\'': ch = '\''; break;
				case '\\': ch = '\\'; break;
				default: ch = start[i + 1];
			}
			
			stride++;
			i++;
		}
		
		buffer[i - stride] = ch;
		len++;
	}
	
	if(len != length) {
		buffer = (char*) reallocate((void*) buffer, length + 1, (len + 1) * sizeof(char));
		length = len;
	}

	buffer[length] = 0;
	
	ObjString* interned = tableFindString(&vm -> strings, buffer, length, hash);

	if(interned != NULL) {
		// Free the buffer as we no longer need it.
		FREE_ARRAY(char, buffer, length + 1);

		return interned;
	}

	return allocateString(vm, buffer, length, true, hash);
}

void printFunction(ObjFunction* function) {
	__printf("<fn '%s' at 0x%.16X>", function -> name == NULL ? "anonymous" : function -> name -> buffer, function);
}

void printObjectRaw(const Value* value) {
	switch(OBJ_TYPE(*value)) {
		case OBJ_STRING: 
			__printf("%s", VALUE_CSTRING(*value));
			break;
		
		case OBJ_FUNCTION: 
			printFunction(VALUE_FUNCTION(*value));
			break;
			
		case OBJ_CLOSURE: {
			ObjClosure* closure = VALUE_CLOSURE(*value);
			
			__printf("<closure '%s' at 0x%.16X with %d upvalues>", closure -> function -> name != NULL ? closure -> function -> name -> buffer : "anonymous", closure -> function, closure -> upvalueCount);
			
			break;
		}
		
		case OBJ_NATIVE: 
			__printf("<fn native at 0x%.16X>", VALUE_NATIVE(*value) -> function);
			break;
		
		case OBJ_UPVALUE: {
			__printf("upvalue(");
			printValue(((ObjUpvalue*) VALUE_OBJECT(*value)) -> location);
			__printf(")");
			
			break;
		}
		
		case OBJ_CLASS: __printf("<class '%s'>", VALUE_CLASS(*value) -> name -> buffer); break;
		
		case OBJ_INSTANCE: __printf("<instance of '%s' class>", VALUE_INSTANCE(*value) -> klass -> name -> buffer); break;
		
		case OBJ_BOUND_METHOD: {
			ObjBoundMethod* method = VALUE_BOUND_METHOD(*value);
			
			switch(method -> function -> type) {
				case OBJ_CLOSURE: {
					ObjClosure* closure = (ObjClosure*) method -> function;
					
					__printf("<closure '%s' at 0x%.16X with %d upvalues", 
						closure -> function -> name != NULL ? closure -> function -> name -> buffer : "anonymous", closure -> function, closure -> upvalueCount);
					
					break;
				}
				
				case OBJ_NATIVE: 
					__printf("<fn native at 0x%.16X", ((ObjNative*) method -> function) -> function);
					break;
				
				case OBJ_FUNCTION: {
					ObjFunction* function = (ObjFunction*) method -> function;
					
					__printf("<fn '%s' at 0x%.16X", function -> name -> buffer, function);
					
					break;
				}
			}
			
			__printf(" of instance ");
			
			switch(method -> receiver.type) {
				case VAL_OBJECT: {
					switch(OBJ_TYPE(method -> receiver)) {
						case OBJ_INSTANCE: {
							__printf("'%s'>", VALUE_INSTANCE(method -> receiver) -> klass -> name -> buffer);
						
							break;
						}

						// Objects which has wrapper classes.

						case OBJ_LIST: {
							__printf("'%s'>", "List");

							break;
						}

						case OBJ_STRING: {
							__printf("'%s'>", "String");

							break;
						}

						case OBJ_DICTIONARY: {
							__printf("'%s'>", "Dictionary");

							break;
						}
					}

					break;
				}
				
				// Refers to a wrapper class.

				case VAL_NUMBER: __printf("'%s'>", "Number"); break;
			}
			
			break;
		}
		
		case OBJ_DICTIONARY: {
			__printf("{");
			
			ObjDictionary* dictionary = VALUE_DICTIONARY(*value);
			
			if(dictionary -> fields.count - dictionary -> fields.tombstones > 0) {	
				__printf(" ");
				
				Value elem;
				int count = 0;
				
				for(register int i = 0; i < dictionary -> fields.capacity; i++) {
					Entry* entry = dictionary -> fields.entries + i;
					
					if(entry -> key != NULL) {
						count++;
						
						__printf("'%s'", entry -> key -> buffer);

						if(entry -> valueContainer.isConst) 
							__printf("(c) : ");
						else __printf(" : ");
						
						elem = entry -> valueContainer.value;
						
						if(IS_STRING(elem)) {
							__printf("'");
							printValue(&elem);
							__printf("'");
						} else printValue(&elem);
						
						if(count < (dictionary -> fields.count - dictionary -> fields.tombstones)) 
							__printf(", ");
					}
				}
				
				__printf(" ");
			}
			
			__printf("}");
			
			break;
		}
		
		case OBJ_LIST: {
			__printf("[");
			
			ObjList* list = VALUE_LIST(*value);
			
			if(list -> count > 0) {
				__printf(" ");
				
				Value elem;
				
				for(register int i = 0; i < list -> count; i++) {
					elem = list -> values[i];
					
					if(IS_STRING(elem)) {
						__printf("'");
						printValue(&elem);
						__printf("'");
					} else printValue(&elem);
					
					if(i + 1 < list -> count) 
						__printf(", ");
				}
				
				__printf(" ");
			}
			
			__printf("]");
			
			break;
		}
		
		case OBJ_FILE: {
			__printf("<File; You weren't supposed to see that!>");
			
			break;
		}
		
		case OBJ_BYTELIST: {
			ObjByteList* byteList = VALUE_BYTELIST(*value);
			
			__printf("<bytelist b'");
			
			for(register size_t i = 0; i < byteList -> size; i++) 
				__printf("\\x%03hhu", byteList -> bytes[i]);
			
			__printf("'>", byteList -> bytes);
			
			break;
		}
	}
}

extern VM* currentVM;

bool printObject(const Value* value) {
	if(IS_INSTANCE(*value)) {
		ObjInstance* instance = VALUE_INSTANCE(*value);

		ValueContainer represent;

		ObjString* name = takeString(currentVM, "__represent__", 13u, false);

		bool found = false;

		if((found = tableGet(&instance -> fields, name, &represent)));
		else if((found = tableGet(&instance -> klass -> methods, name, &represent))) {}

		if(found) {
			Value callable = represent.value;

			if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
				stack_push(currentVM, callable);

				if(callValue(currentVM, callable, 0u)) {
					currentVM -> stack[currentVM -> stackTop - 1u] = *value;

					if(run(currentVM) != INTERPRET_RUNTIME_ERROR) {
						callable = stack_pop(currentVM);

						printValueRaw(&callable);

						return true;
					}

					return false;
				}

				return false;
			}
			else if(IS_NATIVE(callable)) {
				NativeFn native = VALUE_NATIVE(callable) -> function;

				NativePack pack = native(currentVM, 1, (Value*) value);

				if(!pack.hadError) 
					printValueRaw(&pack.value);
				
				return !pack.hadError;
			}
		}
	}
	else if(IS_DICTIONARY(*value)) {
		Table* fields = &VALUE_DICTIONARY(*value) -> fields;

		ObjString* represent = takeString(currentVM, "__represent__", 13u, false);

		ValueContainer valueContainer;

		if(tableGet(fields, represent, &valueContainer)) {
			Value callable = valueContainer.value;

			if(IS_FUNCTION(callable) || IS_CLOSURE(callable)) {
				stack_push(currentVM, callable);

				uint8_t arity = IS_FUNCTION(callable) ? VALUE_FUNCTION(callable) -> arity :
								VALUE_CLOSURE(callable) -> function -> arity;
				
				if(arity >= 1) {
					stack_push(currentVM, *value);

					for(uint8_t i = 0u; i < arity - 1u; i++) 
						stack_push(currentVM, NULL_VAL);
				}

				if(callValue(currentVM, callable, arity) && run(currentVM) != INTERPRET_RUNTIME_ERROR) {
					// Reuse callable.

					callable = stack_pop(currentVM);

					printValueRaw(&callable);

					return true;
				} else return false;
			}
			else if(IS_NATIVE(callable)) {
				NativeFn native = VALUE_NATIVE(callable) -> function;

				NativePack pack = native(currentVM, 1, (Value*) value);

				if(!pack.hadError) {
					printValueRaw(&pack.value);

					return true;
				}
				
				return false;
			}
		}
	}

	printObjectRaw(value);

	return true;
}

ObjFunction* newFunction(VM* vm) {
	ObjFunction* function = ALLOCATE_OBJECT(vm, ObjFunction, OBJ_FUNCTION);

	function -> arity = 0u;
	function -> name  = NULL;
	
	initChunk(&function -> chunk);

	return function;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
	ObjUpvalue** upvalues = (ObjUpvalue**) reallocate(NULL, 0u, sizeof(ObjUpvalue*) * function -> upvalueCount);
	
	for(int i = 0; i < function -> upvalueCount; i++) 
		upvalues[i] = NULL;
	
	ObjClosure* closure = ALLOCATE_OBJECT(vm, ObjClosure, OBJ_CLOSURE);

	closure -> function     = function;
	closure -> upvalues     = upvalues;
	closure -> upvalueCount = function -> upvalueCount;

	return closure;
}

ObjNative* newNative(VM* vm, NativeFn function) {
	ObjNative* native = ALLOCATE_OBJECT(vm, ObjNative, OBJ_NATIVE);
	
	native -> function = function;
	
	return native;
}

ObjUpvalue* newUpvalue(VM* vm, Value* location, bool isConst) {
	ObjUpvalue* upvalue = ALLOCATE_OBJECT(vm, ObjUpvalue, OBJ_UPVALUE);
	
	upvalue -> location = location;
	upvalue -> next     = NULL;
	upvalue -> closed   = NULL_VAL;
	upvalue -> isConst  = isConst;
	
	return upvalue;
}

ObjClass* newClass(VM* vm, ObjString* name) {
	ObjClass* klass = ALLOCATE_OBJECT(vm, ObjClass, OBJ_CLASS);

	klass -> name       = name;
	klass -> superClass = NULL;
	
	initTable(&klass -> methods);
	initTable(&klass -> statics);

	return klass;
}

ObjInstance* newInstance(VM* vm, ObjClass* klass) {
	ObjInstance* instance = ALLOCATE_OBJECT(vm, ObjInstance, OBJ_INSTANCE);

	instance -> klass = klass;

	initTable(&instance -> fields);

	return instance;
}

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, Obj* obj) {
	ObjBoundMethod* boundMethod = ALLOCATE_OBJECT(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
	
	boundMethod -> receiver = receiver;
	boundMethod -> function = obj;
	
	return boundMethod;
}

ObjDictionary* newDictionary(VM* vm) {
	ObjDictionary* dictionary = ALLOCATE_OBJECT(vm, ObjDictionary, OBJ_DICTIONARY);
	
	initTable(&dictionary -> fields);
	
	return dictionary;
}

ObjList* newList(VM* vm) {
	ObjList* list = ALLOCATE_OBJECT(vm, ObjList, OBJ_LIST);
	
	list -> capacity = 0u;
	list -> count    = 0u;
	list -> values     = NULL;
	
	return list;
}

ObjFile* newFile(VM* vm) {
	ObjFile* file = ALLOCATE_OBJECT(vm, ObjFile, OBJ_FILE);
	
	file -> file = NULL;
	
	return file;
}

ObjByteList* newByteList(VM* vm) {
	ObjByteList* byteList = ALLOCATE_OBJECT(vm, ObjByteList, OBJ_BYTELIST);
	
	byteList -> bytes = NULL;
	byteList -> size  = 0u;

	return byteList;
}
