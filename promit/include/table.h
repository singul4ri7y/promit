#ifndef _promit_table_
#define _promit_table_

#pragma once

#include "value.h"

typedef struct {
	ObjString* key;
	ValueContainer valueContainer;
} Entry;

typedef struct {
	size_t count;
	size_t capacity;
	size_t tombstones;
	Entry* entries;
} Table;

void initTable(Table*);
Entry* findEntry(Entry*, size_t, ObjString*);
bool tableInsert(Table*, ObjString*, ValueContainer);
bool tableGet(Table*, ObjString*, ValueContainer*);
void tableInsertAll(Table*, Table*);
bool tableDelete(Table*, ObjString*);
ObjString* tableFindString(Table*, const char*, size_t length, uint32_t);
void markTable(Table*);
void tableRemoveWhite(Table*);
void freeTable(Table*);

#endif
