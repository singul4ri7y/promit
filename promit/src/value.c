#include <math.h>
#include <string.h>

#include "object.h"
#include "lib.h"

void initValueArray(ValueArray* valueArray) {
    valueArray -> capacity = 0u;
    valueArray -> count    = 0u;
    valueArray -> values   = NULL;
}

int writeValueArray(ValueArray* valueArray, Value value) {
    if(valueArray -> count + 1 > valueArray -> capacity) {
        size_t oldCapacity = valueArray -> capacity;

        valueArray -> capacity = GROW_CAPACITY(oldCapacity);
        valueArray -> values   = GROW_ARRAY(Value, valueArray -> values, oldCapacity, valueArray -> capacity);
    }

    valueArray -> values[valueArray -> count++] = value;

    return valueArray -> count - 1u;
}

void freeValueArray(ValueArray* valueArray) {
    FREE_ARRAY(Value, valueArray -> values, valueArray -> capacity);
    initValueArray(valueArray);
}

extern VM* currentVM;

void printValueRaw(const Value* value) {
    switch(value -> type) {
        case VAL_BOOLEAN: 
            __printf(VALUE_BOOL(*value) ? "true" : "false");
            break;
        case VAL_NULL: 
            __printf("null");
            break;
        case VAL_NUMBER: {
            double number = VALUE_NUMBER(*value);
            
            // -0 is the only exception here.
            // '-' will be omitted for '-0' in
            // toString(VM*, Value* const) function.

            if(number == 0) {
                // Now determine whether it's -0 or 0/+0.

                uint64_t uinum;

                memcpy(&uinum, &number, sizeof(double));

                if(uinum & 0xA000000000000000) 
                    __printf("-");
                
                __printf("0");

                break;
            }
            
            char* buffer = toString(currentVM, (Value* const) value);

            __printf(buffer);

            free(buffer);
            
            break;
        }
        case VAL_OBJECT: printObjectRaw(value); break;
    }
}

bool printValue(const Value* value) {
    if(IS_OBJECT(*value)) 
        return printObject(value);
    
    printValueRaw(value);

    return true;
}
