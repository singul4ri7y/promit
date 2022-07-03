#include <string.h>

#include "table.h"
#include "object.h"
#include "memory.h"

#define TABLE_LOAD_FACTOR 0.75

void initTable(Table* table) {
	table -> count      = 0u;
	table -> capacity   = 0u;
	table -> tombstones = 0u;
	table -> entries    = NULL;
}

Entry* findEntry(Entry* entries, size_t capacity, ObjString* key) {
	register uint32_t index = key -> hash & (capacity - 1);
	Entry* tombstone = NULL;

	while(true) {
		Entry* entry = entries + index;

		if(entry -> key == NULL) {
			if(IS_NULL(entry -> valueContainer.value)) 
				return tombstone != NULL ? tombstone : entry;
			else {
				if(tombstone == NULL) 
					tombstone = entry;
			}
		}
		else if(entry -> key == key) 
			return entry;

        // For reasearch purposes. Will be removed in near future.
        //
		// if(entry -> key == key || entry -> key == NULL)
		// 	return entry;

		index = (index + 1) & (capacity - 1);
	}
}

static void adjustCapacity(Table* table, size_t capacity) {
	Entry* entries = GROW_ARRAY(Entry, NULL, 0u, capacity);

	for(register size_t i = 0u; i < capacity; i++) {
		entries[i].key = NULL;
		
		entries[i].valueContainer.isConst = false;
		entries[i].valueContainer.value   = NULL_VAL;
	}

	table -> count = 0;

	for(register int i = 0u; i < table -> capacity; i++) {
		Entry* entry = table -> entries + i;

		if(entry -> key == NULL) continue;

		Entry* dest = findEntry(entries, capacity, entry -> key);

		dest -> key   = entry -> key;
		dest -> valueContainer = entry -> valueContainer;
		table -> count++;
	}

	FREE_ARRAY(Entry, table -> entries, table -> capacity);

	table -> entries  = entries;
	table -> capacity = capacity;
}

bool tableInsert(Table* table, ObjString* key, ValueContainer value) {
	if(table -> count + 1u > table -> capacity * TABLE_LOAD_FACTOR) {
		size_t capacity = GROW_CAPACITY(table -> capacity);
		adjustCapacity(table, capacity);
	}

	Entry* entry = findEntry(table -> entries, table -> capacity, key);

	bool isNewKey = entry -> key == NULL;

	if(isNewKey) {
		if(IS_NULL(entry -> valueContainer.value))
			table -> count++;
		else table -> tombstones--;
	}

	entry -> key = key;
	entry -> valueContainer = value;

	return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
	if(table -> count == 0) 
		return false;

	Entry* entry = findEntry(table -> entries, table -> capacity, key);

	if(entry -> key == NULL) 
		return false;

	entry -> key = NULL;
	entry -> valueContainer.value = BOOL_VAL(true);
	
	table -> tombstones++;

	return true;
}

void tableInsertAll(Table* from, Table* to) {
	for(register int i = 0; i < from -> capacity; i++) {
		Entry* entry = from -> entries + i;

		if(entry -> key != NULL) 
			tableInsert(to, entry -> key, entry -> valueContainer);
	}
}

bool tableGet(Table* table, ObjString* key, ValueContainer* valueContainer) {
	if(table -> count == 0u) 
		return false;

	Entry* entry = findEntry(table -> entries, table -> capacity, key);

	if(entry -> key == NULL) 
		return false;

	*valueContainer = entry -> valueContainer;

	return true;
}

// Fetches a string if it's available as a key in any entry of a certain table.
// Mostly in this projects case, is used to fetch strings for string interning.

ObjString* tableFindString(Table* table, const char* buffer, int length, uint32_t hash) {
	if(table -> count == 0u) 
		return NULL;

	register size_t index = hash & (table -> capacity - 1);

	while(true) {
		Entry* entry = table -> entries + index;

		if(entry -> key == NULL) {
            // If a non-tombstone entry is found, then stop.

			if(IS_NULL(entry -> valueContainer.value)) 
				return NULL;
		}
		else if(entry -> key -> length == length &&
				entry -> key -> hash == hash &&
				!memcmp(entry -> key -> buffer, buffer, length)) {

            // Bingo.
			return entry -> key;
		}

		index = (index + 1) & (table -> capacity - 1);
	}
}

// This function is used to remove unmarked string objects
// global string table.
//
// Usage : tableRemoveWhite(&globalStringInterningTable);

void tableRemoveWhite(Table* table) {
	for(register int i = 0; i < table -> capacity; i++) {
		Entry* entry = table -> entries + i;

		if(entry -> key != NULL && !entry -> key -> obj.isMarked) 
			tableDelete(table, entry -> key);
	}
}

void markTable(Table* table) {
	for(register int i = 0; i < table -> capacity; i++) {
		Entry* entry = table -> entries + i;

		markObject((Obj*) entry -> key);
		markValue(entry -> valueContainer.value);
	}
}

void freeTable(Table* table) {
	FREE_ARRAY(Entry, table -> entries, table -> capacity);
	initTable(table);
}
